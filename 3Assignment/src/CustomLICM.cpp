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
#include "llvm/Analysis/ValueTracking.h"


using namespace llvm;

namespace {

  /**
   * Versione più raffinata della funzione nativa LLVM isLoopInvariant
   * che confronta anche le istruzioni già segnate come loop-invariant
   * 
   * @param I Riferimento all'istruzione del ciclo
   * @param loop Puntatore al loop
   * @param invariants Puntatore all'insieme di istruzioni del ciclo già segnate come loop-invariant
   * @return True se l'istruzione è veramente loop-invariant, false altrimenti
   */
  bool isReallyInvariant(Instruction &I, Loop *loop, SetVector<Instruction*> *invariants) {

    for (Value *Op : I.operands()) {

      // Una costante è invariante
      if (isa<Constant>(Op)) {
        continue;
      }

      // Verifico se l'operando è un'istruzione.
      // Nel caso non lo fosse (es. Argument), è implicito che sia
      // definito fuori dal loop
      if (Instruction *inst = dyn_cast<Instruction>(Op)) {
      
        // L'istruzione è in funzione di un'istruzione loop invariant
        if (invariants->count(inst)) {
          continue;
        }

        // Grazie alla forma SSA, l'istruzione ha una sola definizione.
        // Se questa definizione si trova fuori dal loop, l'operando è invariante.
        if (!loop->contains(inst)) {
          continue;
        }

        return false;
      }
    }
    return true;
  }
  
  /**
   * Dato un loop, ritorna l'insieme delle istruzioni valutate come loop-invariant
   * 
   * @param loop Puntatore al loop
   * @return SetVector<Instruction *> Un insieme ordinato contenente le istruzioni loop-invariant individuate
   */
  SetVector<Instruction *> search_loop_invariant_instructions(Loop *loop) {

    SetVector<Instruction *> invariants; 
    bool isChanged = true;

    /*
    In SSA, la definizione domina sempre l'uso (no nodi phi). 
    Di conseguenza, un'istruzione non utilizzerà mai un operando definito fisicamente
    dopo di essa all'interno dello stesso blocco base.

    Se i blocchi del loop fossero disposti in un ordine topologico perfetto, ovvero dove chi domina viene sempre visitato prima, 
    allora certamente basterebbe una sola passata.

    Il problema principale è che il metodo loop->getBlocks() di LLVM restituisce i blocchi del ciclo 
    in un ordine che non è garantito che sia quello di relazione di dominanza topologica. 
    
    In presenza di ramificazioni all'interno del blocco di codice (es. if/else), può essere che l'IR
    memorizzi internamente i vari blocchi non nell'ordine che ci si aspetti.
    */

    while (isChanged) {
      isChanged = false;

      // Scorro tutti i blocchi del loop
      for (BasicBlock *BB : loop->getBlocks()) {

        for (Instruction &I : *BB) {

          // Se l'istruzione è già stata marcata o è il risultato di un'istruzione phi
          if (invariants.count(&I) || isa<PHINode>(&I))
            continue;

          // Verifica se l'istruzione attuale è veramente loop-invariant
          if (isReallyInvariant(I, loop, &invariants)) {
            invariants.insert(&I);
            isChanged = true;
          }
        }
      }
    }

    return invariants;
  }

  /**
   * Funzione per la verifica delle condizioni per la code motion
   * 
   * @param invariant_instructions Set delle istruzioni invarianti
   * @param loop Puntatore al loop
   * @param DT Dominator Tree
   * @return Vettore di istruzioni candidate (sicure da spostare)
   */
  std::vector<Instruction *>filter_safe_to_move_instructions(
    const SetVector<Instruction *> &invariant_instructions,
    Loop *loop,
    DominatorTree &DT
  ) {

    std::vector<Instruction *> safe_to_move;

    // vado a recuperare i blocchi di uscita dal loop
    SmallVector<BasicBlock *, 8> exit_blocks;
    loop->getExitBlocks(exit_blocks);

    // itero sulle istruzioni identificate come invarianti 
    for (Instruction *I : invariant_instructions) {

      // isTerminator() ritorna true se I è l'ultima istruzione di un blocco
      if (I->isTerminator() || isa<PHINode>(I) || I->mayHaveSideEffects()) continue; // controllo di integrità base: non sposto terminatori, nodi PHI o istruzioni con side effect
      
      // condizione di dominanza delle usite del loop
      bool dominates_all_exits = true;
      BasicBlock *inst_bb = I->getParent(); // identifico il blocco in cui si trova l'istruzione

      // verifico se il blocco dell'istruzione domina tutti i blocchi di uscita
      for (BasicBlock *exit_bb : exit_blocks) {
        if (!DT.dominates(inst_bb, exit_bb)) {
          dominates_all_exits = false;
          break; // basta che non ne domini uno per invalidare la condizione
        }
      }

      // Se NON domina tutte le uscite, controllo se posso salvarla
      if (!dominates_all_exits) {

        /*
        Nonostante la versione standard della code motion preveda strettamente che l'istruzione domini tutte le uscite,
        ne esiste una versione estesa che, in caso di fallimento della prima condizione, la code motion è comunque applicabile
        se la variabile definita dall'istruzione corrente è considerata "morta" al di fuori del loop
        (sempre a patto che anche le altre due condizioni della code motion siano valide).

        In un contesto non SSA, sarebbe necessario applicare l'algoritmo della Liveness Analysis.
        In LLVM, invece, basta iterare sugli usi dell'istruzione e verificare che nessun di essi
        sia contenuto al di fuori del ciclo corrente.

        Oltre alla verifica di inutilizzo della variabile al di fuori del loop, siccome l'istruzione non è stata pensata
        logicamente per essere eseguita a priori prima del loop, si potrebbe verificare il lancio di errori / eccezioni
        nel caso in cui l'istruzione venisse effettivamente spostata.

        Esempio:
        ...
        scanf("%d", &b);  // input = 0
        for (...) {
          if (b != 0)
            x = a / b
        }

        Se la variabile x non venisse utilizzata dopo il loop e decidessi di spostare l'istruzione nel preheader,
        c'è il rischio che l'istruzione eseguirebbe una divisione per 0, lanciando quindi un errore.

        Si richiede, dunque, una verifica anticipata prima della liveness per controllare se l'istruzione è safe.
        */

        if (!isSafeToSpeculativelyExecute(I)) {
          continue;
        }

        bool used_outside_loop = false;

        for (User *user : I->users()) {
          if (Instruction *inst = dyn_cast<Instruction>(user)) {
            if (!loop->contains(inst)) {
              used_outside_loop = true;
              break;
            }
          }
        }

        if (used_outside_loop)
          continue;
      }

      /** 
       * Definizione unica nel loop
       * In SSA questa condizione è sempre era per i registri virtuali. 
       * Il passa lavora solo su Binary Operators, quindi assumiamo sia vera
       */

      // Dominanza degli usi
      bool dominates_all_uses = true;
      // itero su tutti gli utilizzatori dell'istruzione I
      for (User *U : I->users()) {
        if (Instruction *user_inst = dyn_cast<Instruction>(U)){
          // mi interesso solo su gli utilizzatori che si trovano dentro il loop
          if (loop->contains(user_inst)) {
            // controllo se l'istruzione I domina l'utilizzatore U
            if (!DT.dominates(I, user_inst)){
              dominates_all_uses = false;
              break;
            }
          }
        }
      }

      if (!dominates_all_uses) {
        continue;// non domina tutti i suoi usi, allora l'istruzione viene scartata
      }

      // se l'istruzione supera tutti i filtri
      safe_to_move.push_back(I);

    }

    return safe_to_move;

  }

  /** Questa funzione si occuperà di spostare le istruzioni candidate alla code motion.
   * @param preheader : puntatore al preheader del loop (dove sposteremo le istruzioni)
   * @param CandidateInstruction : vettore con all'interno le istruzioni candidate alla code motion
   * Non abbiamo bisogno che la funzione restituisca qualcosa, dato che si deve occupare solamente
   * di spostare le istruzioni che gli vengono passate in input, perciò il tipo sarà void**/
  void moveCandidateInstructions(BasicBlock *preheader,
    std::vector<Instruction *> CandidateInstrucion){

      //puntatore all'ultima istruzione del BB Preheader (di solito una di controllo di flusso)
      Instruction *InsertPT = preheader->getTerminator();

      errs()<<"\n Inizio a spostare le istruzioni \n";
      for(Instruction *I : CandidateInstrucion){
        errs()<<"Istruzione da spostare: ";
        I->print(errs());
        errs()<<"\n";
        errs()<<"BB genitore PRIMA DELLO SPOSTAMENTO:\n";
        I->getParent()->printAsOperand(errs(), false);
        errs()<<"\n";

        //sposta l'istruzione nel preheader;
        //come detto prima, l'ultima istruzione di un BB è un'istruzione di controllo del flusso del programma, quindi spostiamo l'istruzione prima
        //in modo tale che l'istruzione venga eseguita una sola volta prima del loop, senza che venga alterato il controllo di flusso del BB
        I->moveBefore(InsertPT);
        errs()<<"Istruzione spostata\n";
        errs()<<"BB genitore DOPO LO SPOSTAMENTO:\n";
        I->getParent()->printAsOperand(errs(), false);
        errs()<<"\n";
      }
      errs()<<"Istruzioni da spostare finite \n";
  }

  /**
   * Tramite una strategia ricorsiva bottom-up, visita tutti i cicli di un ciclo esterno
   * 
   * @param loop Puntatore al loop
   * @param DT Dominator Tree del programma
   * @return True se è avvenuta almeno una trasformazion all'IR, false altrimenti. 
   */
  bool visitAllLoopsBottonUp(Loop *loop, DominatorTree &DT) {

    bool changed = false;

    // Visito i propri loop annidati
    for (Loop *SubLoop : *loop) {
      changed |= visitAllLoopsBottonUp(SubLoop, DT);
    }

    // Insieme di istruzioni loop-invariant
    SetVector<Instruction *> loop_invariant_instructions = search_loop_invariant_instructions(loop);
    
    for (auto test: loop_invariant_instructions) {
      outs() << "Istruzione: ";
      test->print(outs());
      outs() << "\n";
    }

    std::vector<Instruction *> candidate_instructions = filter_safe_to_move_instructions(loop_invariant_instructions, loop, DT);
    
    // recupero il preheader
    BasicBlock *preheader = loop->getLoopPreheader();
  
    errs() << "Istruzioni filtrate e pronte per la Code Motion:\n";
    if (candidate_instructions.empty()) {
      errs() << "Nessuna istruzione sicura\n";
    } else {
      for (Instruction *I : candidate_instructions) {
        errs() << " -> " << *I << "\n";
      }
    }
  
    if (preheader && !candidate_instructions.empty()){
      changed = true;
      moveCandidateInstructions(preheader, candidate_instructions);
    }

    return changed;
  }
  

  struct CustomLICM : PassInfoMixin<CustomLICM> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

      // CFG - Itera sui cicli più esterni
      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
      // Albero dei dominatori - Valuta le condizioni per la code motion
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

      // Verifica se il CFG contiene almeno un loop
      if (LI.empty()) {
        errs() << "\nNella funzione non ci sono loop!\n";
        return PreservedAnalyses::all();
      }

      // Determina se è stata applicata almeno una code motion
      bool has_licm_been_applicated = false;

      // Scorre tutti i Loop del CFG
      for (Loop *loop : LI) {        
        has_licm_been_applicated |= visitAllLoopsBottonUp(loop, DT);
      }
      
      if (has_licm_been_applicated)
        return PreservedAnalyses::none();
      else
        return PreservedAnalyses::all();
    }

  };

  static bool isRequired() { return true; }
};

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getCustomLICMPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CustomLICM", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "custom-licm") {
                    FPM.addPass(CustomLICM());
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
  return getCustomLICMPluginInfo();
}