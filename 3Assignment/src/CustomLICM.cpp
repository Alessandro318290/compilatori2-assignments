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

  
  SetVector<Instruction *> search_loop_invariant_instructions(Loop *loop);

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
        if (I->isTerminator() || isa<PHINode>(I)) continue; // controllo di integrità base: non sposto terminatori o nodi PHI
      
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

  // Dato il loop più esterno, li visito tutti dal più interno al più esterno
  void visitAllLoopsBottonUp(Loop *loop, DominatorTree &DT) {

    for (Loop *SubLoop : *loop) {
        visitAllLoopsBottonUp(SubLoop, DT);
    }

    SetVector<Instruction *> loop_invariant_instructions = search_loop_invariant_instructions(loop);

    std::vector<Instruction *> candidate_instructions =
      filter_safe_to_move_instructions(loop_invariant_instructions, loop, DT);
    
    // recupero il preheader da dare come input a leo insieme al vettore
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
      // resto del codice qua
    }
  }

  // New PM implementation
  struct CustomLICM : PassInfoMixin<CustomLICM> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

      // Verifico se il CFG contiene almeno un loop
      if (LI.empty()) {
          errs() << "\nNella funzione non ci sono loop!\n";
          return PreservedAnalyses::all();
      }

      // Scorro tutti i Loop del CFG
      for (Loop::iterator lit = LI.begin(); lit != LI.end(); lit++)
      {          
          Loop *loop = *lit;

          visitAllLoopsBottonUp(loop, DT);
      }
      
      return PreservedAnalyses::none();
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