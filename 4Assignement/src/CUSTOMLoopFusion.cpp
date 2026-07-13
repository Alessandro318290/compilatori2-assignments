#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/BasicBlock.h"

#include "llvm/ADT/SmallVector.h"

using namespace llvm;

namespace {

  BasicBlock *getLoopGuard(Loop *L){
    BranchInst *BI = L->getLoopGuardBranch();

    if(!BI)
      return nullptr;
 
    return BI->getParent();
  }

  bool hasUnsafeSideEffectCall(Loop *L) {

    for (BasicBlock *BB : L->blocks()) {
      for (Instruction &I : *BB) {

        if (auto *CI = dyn_cast<CallInst>(&I)) {

          if (CI->mayHaveSideEffects()) {
            return true;
          }
        }
      }
    }

    return false;
  }

  /**
   * Tramite una strategia ricorsiva bottom-up, visita tutti i cicli di un ciclo esterno
   * 
   * @param loop Puntatore al loop corrente
   * @param DT Dominator Tree della funzione
   * @param PDT PostDominator della funzione
   * @param SE Scalar Evolution analysis
   * @param prev Puntatore al loop fratello precedente
   * @param LI LoopInfo
   * @param DI Analisi delle dipendenze
   * @return True se è avvenuta almeno una trasformazion all'IR, false altrimenti. 
   */
  bool visitAllLoopsBottonUp(Loop *&loop, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE, Loop *prev, LoopInfo &LI, DependenceInfo &DI) {

    bool changed = false;

    // Visito i loop annidati del loop corrente
    Loop *prevSub = nullptr; 

    for (Loop *SubLoop : loop->getSubLoops()) {
      changed |= visitAllLoopsBottonUp(SubLoop, DT, PDT, SE, prevSub, LI, DI);

      if (changed)
        break;

      prevSub = SubLoop;
    }

    BasicBlock *loopHeader = loop->getHeader();
    outs() << "\nAnalisi loop corrente: ";
    loopHeader->printAsOperand(outs(), false);

    if(prev == nullptr){ // Se prev è vuoto, non abbiamo nessun altro loop con cui fondere
      errs()<<"\nLoop precedente non trovato. Ritorno al livello precedente \n";
      return changed;
    }

    BasicBlock *prevHeader = prev->getHeader();
    outs() << "\nAnalisi loop precedente: ";
    prevHeader->printAsOperand(outs(), false);

    /*
     * Si procede se e soltanto se i due loop sono entrambi guarded o non guarded.
     * In caso contrario fallirebbe la condizione di Control Flow Equivalence.
     */
    if((prev->isGuarded() != loop->isGuarded())){
      errs()<<"\nTipi di loop diversi \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    outs()<<"\n\nStampa dei BB del loop precedente: \n";

    // Stampa dei BB del loop precedente
    for (BasicBlock *BB : prev->blocks()) {
      BB->print(outs());
      outs() << "\n";
    }

    outs()<<"\nStampa dei BB del loop corrente: \n";

    // Stampa dei basic block del loop corrente
    for (BasicBlock *BB : loop->blocks()) {
      BB->print(outs());
      outs() << "\n";
    }

    // VERIFICA DELLE CONDIZIONI DELLA LOOP FUSION

    // (1) Adiacenza

    bool areAdjacent = false;

    BasicBlock *prevExit = prev->getUniqueExitBlock(); // Si prende il blocco di uscita del loop prev
    if(!prevExit){
      outs()<<"\nBlocco di uscita non trovato \n";
      outs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    outs() << "\nNome blocco prevExit di prev: ";
    if (prevExit) 
      prevExit->printAsOperand(outs(), false);
    else 
      outs() << "nullptr";
    
    outs() << "\nNome Preheader di loop: ";
    if (loop->getLoopPreheader()) 
      loop->getLoopPreheader()->printAsOperand(outs(), false);
    else 
      outs() << "nullptr";
    
    outs() << "\nNome Header di loop: ";
    loop->getHeader()->printAsOperand(outs(), false);
    outs() << "\n";

    if(loop->isGuarded()){ // Caso guarded

      outs()<<"\nI loop sono guarded \n";

      BasicBlock *Guard = getLoopGuard(loop);
      if(!Guard){
        outs()<<"\nGuard branch non trovato per il secondo loop \n";
        outs()<<"\nRitorno al livello precedente \n";
        return changed;
      }

      areAdjacent = (prevExit == Guard);

    }else{

      outs()<<"\nI loop non sono guarded \n";
      areAdjacent = (prevExit == loop->getLoopPreheader());
    }

    if(!areAdjacent){
      outs()<<"\nI loop non sono adiacenti\n";
      outs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    outs()<<"\nI due loop sono adiacenti \n";
    
    // (2) Trip Count

    // Si verifica che i due loop abbiano lo stesso numero di iterazioni.
    // Si usa l'analisi Scalar Evolution per determinare matematicamente 
    // l'evoluzione delle variabili di induzione e garantire l'equivalenza dello spazio delle iterazioni.

    // Per entrambi i loop si estrapola il numero di volte che il flusso d'esecuzione
    // attraversa il backedge per ricominciare il ciclo
    const SCEV *BTC1 = SE.getBackedgeTakenCount(prev);
    const SCEV *BTC2 = SE.getBackedgeTakenCount(loop);

    // Per entrambi i loop verifichiamo se il numero di iterazioni è possibile determinarlo staticamente.
    // Nel caso in cui il numero di iterazioni dipenda da input dinamici non prevedibili,
    // la SCEV non è in grado di calcolarlo e restituisce un oggetto di tipo SCEVCouldNotCompute

    bool CanCompute1 = !isa<SCEVCouldNotCompute>(BTC1); 
    if(!CanCompute1){
      errs()<<"\nNon è stato possibile determinare il trip count del primo loop\n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }
    
    bool CanCompute2 = !isa<SCEVCouldNotCompute>(BTC2);
    if(!CanCompute2){
      errs()<<"\nNon è stato possibile determinare il trip count del secondo loop\n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }
    
    //ICMP_EQ -> BTC1 == BTC2, restituirà true solo se la scalar evolution potrà provare matematicamente che BTC1 = BTC2
    bool SameCount = SE.isKnownPredicate(ICmpInst::ICMP_EQ, BTC1, BTC2);

    if(!SameCount){
      errs()<<"\nI due loop hanno un trip count diverso";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    outs()<<"\nI due loop iterano lo stesso numero di volte \n";

    // (3) Control Flow Equivalence

    // prev dom loop
    // Ogni percorso dall'entry block a Loop passa per prev

    // loop pdom prev
    // Ogni percorso da prev all'exit block del programma passa per loop

    if(!DT.dominates(prev->getHeader(), loop->getHeader())){
      outs()<<"\nIl primo loop NON domina il secondo \n";
      outs()<<"\nRitorno al livello precedente \n";  
      return changed;
    }

    outs()<<"\nIl primo loop DOMINA il secondo";

    if(!PDT.dominates(loop->getHeader(), prev->getHeader())){
      outs()<<"\nIl secondo loop NON post-domina il primo \n";
      outs()<<"\nRitorno al livello precedente";
      return changed;
    }

    outs()<<"\nIl secondo loop POST-DOMINA il primo\n";

    /*
    Evito di fondere loop aventi delle chiamate a funzioni con side effect

    Basti pensare a due semplici loop con stampa dell'iteratore

    for (i = 0; i < 5; i++)
      printf("%d", i);
    for (i = 0; i < 5; i++)
      printf("%d", i);

    All'apparenza non sembrerebbe violare le condizioni della loop fusion.
    In realtà, però, la fusione violerebbe la semantica del programma.
    
    Output prima della loop fusion:
    0123401234

    Output con l'eventuale fusione dei loop:
    0011223344

    Si otterrebbero output differenti
    */

    if(hasUnsafeSideEffectCall(prev) || hasUnsafeSideEffectCall(loop)){
      outs()<<"\nPresente una call con side effect: fusione non applicabile";
      outs()<<"\nRitorno al livello precedente \n";
      return changed;
    }
    
    // (4) Dependence Analysis

    PHINode *IVPrev = prev->getCanonicalInductionVariable();
    PHINode *IVLoop = loop->getCanonicalInductionVariable();

    // Itero su tutte le istruzioni del primo loop (prev)
    for(BasicBlock *BB: prev->blocks()){
      for(Instruction &I: *BB){

        // Si è interesati alle sole istruzioni Load/store,
        // poiché sono quelle che possono generare dipendenze
        if(!isa<LoadInst>(I) && !isa<StoreInst>(I))
          continue;

        // Una volta trovata un'istruzione load/store nel primo loop,
        // itero per trovare un'altra istruzione load/store che accede allo stesso elemento
        // generando così una dipendenza
        for(BasicBlock *BB2: loop->blocks()){
          for(Instruction &I2: *BB2){

            if(!isa<LoadInst>(I2) && !isa<StoreInst>(I2))
              continue;

            // Due Load allo stesso elemento non bloccano la Loop Fusion
            if (isa<LoadInst>(I) && isa<LoadInst>(I2))
              continue;

            // Recupero l'oggetto che contiene l'eventuale analisi della dipendenza fra I1 e I2
            auto dep = DI.depends(&I, &I2, true);
            if(!dep)
              continue;
            
            // Recupero il base address dell'oggetto in memoria che crea dipendenza
            Value *Ptr = getLoadStorePointerOperand(&I);
            Value *Ptr2 = getLoadStorePointerOperand(&I2);

            if (!Ptr || !Ptr2) continue;

            // Valuto le SCEV rispetto allo stesso punto di osservazione esterno ai loop
            const SCEV *S1 = SE.getSCEVAtScope(Ptr, nullptr);
            const SCEV *S2 = SE.getSCEVAtScope(Ptr2, nullptr);

            if (!S1 || !S2) continue;

            // Calcolo la distanza
            const SCEV *Diff = SE.getMinusSCEV(S1, S2);

            if (!Diff || isa<SCEVCouldNotCompute>(Diff)) {
              outs() << "\nImpossibile calcolare algebricamente la distanza SCEV, loop fusion non applicabile\n";
              return changed;
            }

            if(SE.isKnownNegative(Diff)){
              //dipendenza backward -> loop fusion non può essere applicata
              outs()<<"\nDipendeza backward, loop fusion non applicabile";
              outs()<<"\nRitorno al livello precedente \n";
              return changed;
            }

            if(!SE.isKnownNonNegative(Diff)){
              //incerto
              outs()<<"\nDipendenza incerta, loop fusion non applicabile";
              outs()<<"\nRitorno al livello precedente \n";
              return changed;
            }
          }
        }
      }
    }

    outs()<<"\nNon sono state trovate istruzioni rischiose nei loop\n";

    // FASE DI TRASFORMAZIONE

    outs()<<"\nTutte le condizioni per la loop fusion verificate, possiamo fondere i loop \n";

    //Per prima cosa modifichiamo gli usi della induction variable di loop con quelli di prev
    outs()<<"\nModifica degli usi delle IV \n";

    for(BasicBlock *BB : loop->blocks()){ 
      for(Instruction &I : *BB){ 
        for(Use &U : I.operands()){ 
          if(U.get() == IVLoop)
            U.set(IVPrev);
        }
      }
    }

    //Ora non ci rimane che modificare il CFG, agganciando il body di prev a quello di loop

    outs()<<"\nModifica del CFG \n";

    BasicBlock *prevLatch = prev->getLoopLatch();
    BasicBlock *loopLatch = loop->getLoopLatch();
    BasicBlock *loopExit = loop->getUniqueExitBlock();

    // Se gli oggetti non sono definiti allora i loop non sono in forma normale
    if (!prevLatch || !loopLatch || !loopExit) {
      errs() << "\nI loop non sono in forma normale";
      return changed;
    }

    BasicBlock *prevBodyEntry = nullptr;
    for(BasicBlock *succ : successors(prevHeader)){

      if(prev->contains(succ)){
        prevBodyEntry = succ; 
        break;
      }

    }    

    BasicBlock *loopBodyEntry = nullptr;
    for(BasicBlock *Succ : successors(loopHeader)){

      if(loop->contains(Succ)){
        loopBodyEntry = Succ; 
        break;
      }

    }
 
    if (!prevBodyEntry || !loopBodyEntry) {
      errs() << "\nErrore nell'identificazione del blocco di ingresso del body dei loop";
      return changed;
    }

    // Si collegano i predecessori di prevLatch a loopBodyEntry
    for (BasicBlock *pred : predecessors(prevLatch)) {
      
      Instruction *predTerminator = pred->getTerminator();

      for (unsigned i = 0; i < predTerminator->getNumSuccessors(); i++) {

        if (predTerminator->getSuccessor(i) == prevLatch) {

          // Si collega il blocco all'entry body di loop
          predTerminator->setSuccessor(i, loopBodyEntry);
          
          /*
          Siccome il body di loop non verrà più raggiunto da loopHeader ma da prev,
          è necessario aggiornare questa informazione dentro loop
          */
          for (PHINode &phi : loopBodyEntry->phis()) {

            // Ritorna l'indice del blocco header di loop dentro il vettore incomingBlocks del nodo phi
            int idx = phi.getBasicBlockIndex(loopHeader);

            if (idx != -1) {
              // Il loop pred prende il posto di loopHeader mantenendo invariato il suo valore
              phi.setIncomingBlock(idx, pred); 
            }
          }
        }
      }
    }

    // Si collegano i predecessori di LoopLatch a prevLatch
    for (BasicBlock *pred : predecessors(loopLatch)) {

      Instruction *predT = pred->getTerminator();

      for (unsigned i = 0; i < predT->getNumSuccessors(); i++) {

        if (predT->getSuccessor(i) == loopLatch) {

          predT->setSuccessor(i, prevLatch);
        }
      }
    }

    // Si collega l'uscita di prevHeader all'uscita di loopHeader
    Instruction* prevHeaderT = prevHeader->getTerminator();

    // Scorriamo i successori del branch nell'header
    for (unsigned i = 0; i < prevHeaderT->getNumSuccessors(); i++) {
      
      BasicBlock *succ = prevHeaderT->getSuccessor(i);

      if (!prev->contains(succ)) {
        prevHeaderT->setSuccessor(i, loopExit);
        break;
      }
    }

    outs() << "\nLoop fusion e pulizia CFG completate con successo!\n";

    loop = prev;

    return true;
  }

  struct CUSTOMLoopFusion : PassInfoMixin<CUSTOMLoopFusion> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
      outs() << "\n***********************************\n";
      outs() << "\nSto analizzando la funzione: " << F.getName() << " \n";
      
      // CFG - Itera sui cicli più esterni
      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
      // Albero dei dominatori - Valuta le condizioni per la code motion
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
      //Albero di postdominanza
      PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
      //Scalar evolution
      ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
      //Dependence Analysis
      DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
      
      // Verifica se il CFG contiene almeno un loop
      if (LI.empty()) {
        outs() << "\nNella funzione non ci sono loop!\n";
        return PreservedAnalyses::all();
      }

      // Determina se è stata applicata almeno una loop fusion
      bool has_loop_fusion_been_applicated = false;

      // Scorre tutti i Loop del CFG
      Loop *prev = nullptr;
      SmallVector<Loop *, 8> loops = LI.getLoopsInPreorder();

      for (Loop *loop : loops) {

        // Salto i loop interni, verranno processatri dentro visitAllLoopsBottonUp
        if (loop->getParentLoop() != nullptr)
          continue;

        has_loop_fusion_been_applicated |= visitAllLoopsBottonUp(loop, DT, PDT, SE, prev, LI, DI);

        if (has_loop_fusion_been_applicated) {
          break; 
        }

        prev = loop;
      }

      if(has_loop_fusion_been_applicated){
        return PreservedAnalyses::none();
      }
      
      outs()<<"\nNessuna modifica ai loop effettuata in questa funzione \n";
      return PreservedAnalyses::all();
    }

  };

  static bool isRequired() { return true; }
};

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getCUSTOMLoopFusionPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CUSTOMLoopFusion", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "custom-loopFusion") {
                    FPM.addPass(CUSTOMLoopFusion());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LoopPass when added to the pass pipeline on the
// command line, i.e. via '-passes=test-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getCUSTOMLoopFusionPluginInfo();
}