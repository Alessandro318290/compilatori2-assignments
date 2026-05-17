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

      // L'operando è un'istruzione se il cast non ritorna null
      Instruction *OpInst = dyn_cast<Instruction>(Op);

      // bool Loop::isLoopInvariant(const Value *V) const {
      //   if (const Instruction *I = dyn_cast<Instruction>(V))
      //     return !contains(I);
      //   return true; // All non-instructions are loop invariant
      // }
      
      /* 
      Un operando NON è invariante se è definito all'interno del ciclo ma 
      l'istruzione che lo definisce non è già stata segnata come invariante
      */
      if (!loop->isLoopInvariant(Op) && (!OpInst || !invariants->count(OpInst))) {
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

  // Funzione per la verifica delle condizioni per la code motion
  /** prende in unput il set delle istruzioni invarianti (MICH), il loop e il DominatorTree
   *  restituisce il vettore di istruzione candidate (sicure da spostare) 
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
    // itero sulle istruzioni identificate come invarianti dalla parte di MICH
    for (Instruction *I : invariant_instructions) {
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

      // Se NON domina tutte le uscite, controlliamo se possiamo salvarla
      if (!dominates_all_exits) {
        // Se è un BinaryOperator (add, mul, sub...), è un'istruzione intrinsecamente "safe"
        // che non genera eccezioni. Quindi possiamo sollevarla anche se il blocco non domina le uscite.
        if (!isa<BinaryOperator>(I)) {
          continue; // Se non è un BinOp e non domina le uscite, la scartiamo definitivamente
        }
      }

      /** Definizione unica nel loop
       *  In SSA questa condizione è sempre era per i registri virtuali. 
       *  Il passa lavora solo su Binary Operators, quindi assumiamo sia vera
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

      // se l'istruzione supera tutti i filtri, viene aggiunta all'output per Leo
      safe_to_move.push_back(I);

    }

    return safe_to_move;

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
  
    // Leo qua dovrà prendere candidate_instructions e preheader
    if (preheader && !candidate_instructions.empty()){
      changed = true;
      // resto del codice qua
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