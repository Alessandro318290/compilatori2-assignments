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

    // Resto del codice qui
    // Prima di terminare il programma, impostare chaned = true se c'è almeno un'istruzione code motion, ovvero se è stata spostata

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