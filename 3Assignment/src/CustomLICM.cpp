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

  // New PM implementation
  struct CustomLICM : PassInfoMixin<CustomLICM> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

      // Verifico se il CFG contiene almeno un loop
      if (LI.empty()) {
          errs() << "\nNella funzione non ci sono loop!\n";
          return PreservedAnalyses::all();
      }
      
      // Scorro tutti i Loop del CFG
      for (Loop::iterator lit = LI.begin(); lit != LI.end(); lit++)
      {          
          Loop *loop = *lit;

          //Codice qui

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