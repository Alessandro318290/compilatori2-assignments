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

    BasicBlock *GuardBB = BI->getParent();

    return GuardBB;
  }

  /**
   * Tramite una strategia ricorsiva bottom-up, visita tutti i cicli di un ciclo esterno
   * 
   * @param loop Puntatore al loop
   * @param DT Dominator Tree del programma
   * @return True se è avvenuta almeno una trasformazion all'IR, false altrimenti. 
   */
  bool visitAllLoopsBottonUp(Loop *&loop, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE, Loop *prev, LoopInfo &LI, DependenceInfo &DI, Function &F) {

    bool changed = false;

    // Visito i propri loop annidati
    Loop *prevS = nullptr;
    Loop *&prevSub = prevS; //puntatore per memorizzare il loop precedente
    for (Loop *SubLoop : loop->getSubLoops()) {
      changed |= visitAllLoopsBottonUp(SubLoop, DT, PDT, SE, prevSub, LI, DI, F);

      prevSub = SubLoop;
      if(changed)
        return true;
    }

    if(prev == nullptr){ //se prev è vuoto, non abbiamo nessun altro loop con cui fondere
      errs()<<"\nLoop precedente non trovato \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    if((prev->isGuarded() != loop->isGuarded())){ //Inanzitutto controlliamo se i loop sono entrambi dello stesso tipo: guarded o NON guarded
      errs()<<"\nTipi di loop diversi \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    errs()<<"\nLoop prev: \n";
    for (BasicBlock *BB : prev->blocks()) {
      errs() << "BasicBlock: " << BB->getName() << "\n";
      BB->print(errs());
      errs() << "\n";
    }

    errs()<<"\nLoop now: \n";
    for (BasicBlock *BB : loop->blocks()) {
      errs() << "BasicBlock: " << BB->getName() << "\n";
      BB->print(errs());
      errs() << "\n";
    }

    //Inizio ora l'analisi dell'adiacenza dei loop
    bool areAdjacent = false;
    BasicBlock *Exit = prev->getUniqueExitBlock(); //prendiamo il blocco di uscita del loop prev
    if(!Exit){
      errs()<<"\nBlocco di uscita non trovato \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }
    if(loop->isGuarded()){ //caso guarded
      errs()<<"\n I loop sono guarded \n";
      areAdjacent = (Exit == getLoopGuard(loop));
    }else{
      errs()<<"\n I loop NON sono guarded \n";
      areAdjacent = (Exit == loop->getLoopPreheader()); //caso NON guarded
    }

    if(!areAdjacent){
      errs()<<"\n I loop non sono adiacenti\n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    errs()<<"\nI due loop sono adiacenti \n";
    //Inizio analisi loop trip count
    //I loop devono iterare lo stesso numero di volte
    //Anche se viene usato per altri scopi, per verificare ciò useremo la scalar evolution.
    //Andremmo ossia a studiare il cambiamento nel valore delle variabili scalari nelle iterazioni dei loop

    const SCEV *BTC1 = SE.getBackedgeTakenCount(prev);
    const SCEV *BTC2 = SE.getBackedgeTakenCount(loop);

    bool CanCompute1 = isa<SCEVCouldNotCompute>(BTC1); //Controlliamo se non è possibile determinare il numero di iterazioni
    bool CanCompute2 = isa<SCEVCouldNotCompute>(BTC2);
    bool SameCount = SE.isKnownPredicate(ICmpInst::ICMP_EQ, BTC1, BTC2);//ICMP_EQ -> BTC1 == BTC2, restituirà true solo se la scalar evolution potrà provare matematicamente che BTC1 = BTC2

    if(CanCompute1){
      errs()<<"\n Non è stato possibile determinare il trip count del primo loop\n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }
    
    if(CanCompute2){
      errs()<<"\n Non è stato possibile determinare il trip count del secondo loop\n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    if(!SameCount){
      errs()<<"\nI due loop hanno un trip count diverso \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    errs()<<"\n I due loop iterano lo stesso numero di volte \n";

    //Inizio analisi control flow dei loop
    //Per essere fusibili prev deve dominare loop
    //e loop deve post-dominare prev
    if(!DT.dominates(prev->getHeader(), loop->getHeader())){
      errs()<<"\n Il primo loop NON domina il secondo \n";
      errs()<<"\nRitorno al livello precedente \n";  
      return changed;
    }

    errs()<<"\n Il primo loop DOMINA il secondo \n";

    if(!PDT.dominates(loop->getHeader(), prev->getHeader())){
      errs()<<"\n Il secondo loop NON post-domina il primo \n";
      errs()<<"\nRitorno al livello precedente \n";
      return changed;
    }

    errs()<<"\n Il secondo loop POST-DOMINA il primo \n";
    
    //Inizio controllo Dependence Analysis
    for(BasicBlock *BB: prev->blocks()){
      for(Instruction &I: *BB){
        if(!isa<LoadInst>(I) && !isa<StoreInst>(I))
          continue;
        for(BasicBlock *BB2: loop->blocks()){
          for(Instruction &I2: *BB2){ //Le istruzioni che sono di nostro interesse sono le load/store
            if(!isa<LoadInst>(I2) && !isa<StoreInst>(I2))
              continue;
            auto dep = DI.depends(&I, &I2, true);
            Value *Ptr = getLoadStorePointerOperand(&I);
            Value *Ptr2 = getLoadStorePointerOperand(&I2);

            const SCEV *S1 = SE.getSCEVAtScope(Ptr, prev);
            const SCEV *S2 = SE.getSCEVAtScope(Ptr2, loop);

            const SCEV *Diff = SE.getMinusSCEV(S1, S2);

            if(SE.isKnownNegative(Diff)){
              //dipendenza backward -> loop fusion non può essere applicata
              errs()<<"\n Dipendeza backward, loop fusion non applicabile\n";
              errs()<<"\nRitorno al livello precedente \n";
              return changed;
            }

            if(!SE.isKnownNonNegative(Diff)){
              //incerto
              errs()<<"\n Dipendenza incerta, loop fusion non applicabile\n";
              errs()<<"\nRitorno al livello precedente \n";
              return changed;
            }
          }
        }
      }
    }

    errs()<<"\n Non sono state trovate istruzioni rischiose nei loop\n";

    //Arrivati a questo punto tutte le condizioni per la loop fusion sono verificate, possiamo fondere i loop
    errs()<<"\n Tutte le condizioni per la loop fusion verificate, possiamo fondere i loop \n";

    //Per prima cosa: modifichiamo gli usi della induction variable di loop con quelli di prev
    errs()<<"\n Modifica degli usi delle IV \n";
    PHINode *IVPrev = prev->getCanonicalInductionVariable();
    PHINode *IVLoop = loop->getCanonicalInductionVariable();

    for(BasicBlock *BB : loop->blocks()){ //Volendo potremmo fare semplicemente IVLoop->replaceAllUsesWith(IVPrev),
      for(Instruction &I : *BB){ //ma meglio sostituire gli usi solo nelle istruzioni dentro a prev,
        for(Use &U : I.operands()){ //per evitare di compromettere del codice più in avanti
          if(U.get() == IVLoop)
            U.set(IVPrev);
        }
      }
    }

    //Ora non ci rimane che modificare il CFG, agganciando il body di prev a quello di loop

    errs()<<"\n Modifica del CFG \n";

    BasicBlock *HeaderPrev = prev->getHeader(); //prendiamo alcuni BB che ci serviranno
    BasicBlock *LatchPrev = prev->getLoopLatch();

    BasicBlock *HeaderLoop = loop->getHeader();

    //recuperiamo il BB body di prev

    BasicBlock *BodyPrevEntry = nullptr;
    for(BasicBlock *Succ : successors(HeaderPrev)){
      if(prev->contains(Succ) && Succ != HeaderPrev){
        BodyPrevEntry = Succ; //abbiamo trovato il body
        break;
      }
    }    

    //ora recuperiamo il BB body di loop

    BasicBlock *BodyLoopEntry = nullptr;

    for(BasicBlock *Succ : successors(HeaderLoop)){
      if(loop->contains(Succ) && Succ != HeaderLoop){
        BodyLoopEntry = Succ; //abbiamo trovato il body
        break;
      }
    }

    //colleghiamo il body di prev con il body di loop
    auto *TIPrev = BodyPrevEntry->getTerminator();

    for(unsigned i=0; i< TIPrev->getNumSuccessors(); i++){
      if(TIPrev->getSuccessor(i) == LatchPrev){
        TIPrev->setSuccessor(i, BodyLoopEntry);
      }
    }

    //colleghiamo il body di loop con il latch di prev
    BasicBlock *LatchLoop = loop->getLoopLatch();
    for(BasicBlock *Pred : predecessors(LatchLoop)){
      if(!loop->contains(Pred))
        continue;
      
      auto *TI = Pred->getTerminator();

      for(unsigned i = 0; i < TI->getNumSuccessors(); i++){
        if(TI->getSuccessor(i) == LatchLoop){
          TI->setSuccessor(i, LatchPrev);
        }
      }
    }

    //agganciamo l'header di prev all'uscita di loop
    HeaderPrev = prev->getHeader();  // header del loop fuso
    BasicBlock *ExitLoop = loop->getUniqueExitBlock(); // uscita del loop assorbito

    auto *Term = HeaderPrev->getTerminator();

    // Scorriamo i successori del branch nell'header
    for (unsigned i = 0; i < Term->getNumSuccessors(); i++) {
        BasicBlock *Succ = Term->getSuccessor(i);

        // Se il successore era l'header/latch del loop assorbito, lo ricolleghiamo all'uscita
        if (loop->contains(Succ)) {
            Term->setSuccessor(i, ExitLoop);
        }
    }

    loop = prev;

    errs()<<"\nLoop fusion completata, prossimo loop \n";
    return true;
  }

  struct CUSTOMLoopFusion : PassInfoMixin<CUSTOMLoopFusion> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
      errs()<<"\n ----Sto analizzando la funzione: "<<F.getName()<<" \n";
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
          errs() << "\nNella funzione non ci sono loop!\n";
          return PreservedAnalyses::all();
      }

      // Determina se è stata applicata almeno una loop fusion
      bool has_loop_fusion_been_applicated = false;

      // Scorre tutti i Loop del CFG
      Loop *p = nullptr;
      Loop *&prev = p;//puntatore per memorizzare il loop precedente

      SmallVector<Loop *, 8> Loops = LI.getLoopsInPreorder();

      for (Loop *loop : Loops) {        
        has_loop_fusion_been_applicated |= visitAllLoopsBottonUp(loop, DT, PDT, SE, prev, LI, DI, F);
        prev = loop;
        if(has_loop_fusion_been_applicated){
          errs()<<"\n E' stata applicata la loop fusion - far ripartire il programma...\n";
          errs()<<" ...per avere le tabelle aggiornate \n";
          return PreservedAnalyses::none();
        }
      }
      
      if (has_loop_fusion_been_applicated){
        return PreservedAnalyses::none();
        errs()<<"\n E' stata applicata la loop fusion - far ripartire il programma...\n";
          errs()<<" ...per avere le tabelle aggiornate \n";
          /*NOTA: ho provato ad aggiornare manualmente le tabelle di dominanza, postdominanza e loop info
          ma il passo crashava, facendo come stiamo facendo ora il passo si ferma ogni volta che effettua una loop fusion
          costringendo a dover far ripartire il passo; in questo modo le tabelle vengono automaticamente aggiornate durante la
          nuova esecuzione del passo*/
      }else{
        errs()<<"\n Nessuna modifica ai loop effettuata in questa funzione \n";
        return PreservedAnalyses::all();
      }
      errs()<<"\n Nessuna modifica ai loop effettuata in questa funzione \n";
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