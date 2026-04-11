#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// PrimoPasso implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {


    // New PM implementation
    struct PrimoPasso: PassInfoMixin<PrimoPasso> {
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

            for (auto &BB : F) {
                for (auto &I : BB) {
                    // Controllo se è una ADD
                    if (I.getOpcode() == Instruction::Add) {
                        Value *LHS = I.getOperand(0);
                        Value *RHS = I.getOperand(1);

                        // x + 0 => x
                        if (ConstantInt *C = dyn_cast<ConstantInt>(RHS)) {
                            if (C->isZero()) {
                                errs() << "Algebraic Identity: " << I << " => ";
                                I.replaceAllUsesWith(LHS);
                                errs() << *LHS << "\n";
                            }
                        }
                        // 0 + x => x
                        if (ConstantInt *C = dyn_cast<ConstantInt>(LHS)) {
                            if (C->isZero()) {
                                errs() << "Algebraic Identity: " << I << " => ";
                                I.replaceAllUsesWith(RHS);
                                errs() << *RHS << "\n";
                            }
                        }
                    }

                    // Controllo se è una MUL
                    if (I.getOpcode() == Instruction::Mul) {
                        Value *LHS = I.getOperand(0);
                        Value *RHS = I.getOperand(1);

                        // x * 1 => x
                        if (ConstantInt *C = dyn_cast<ConstantInt>(RHS)) {
                            if (C->isOne()) {
                                errs() << "Algebraic Identity: " << I << " => ";
                                I.replaceAllUsesWith(LHS);
                                errs() << *LHS << "\n";
                            }
                        }
                        // 1 * x => x
                        if (ConstantInt *C = dyn_cast<ConstantInt>(LHS)) {
                            if (C->isOne()) {
                                errs() << "Algebraic Identity: " << I << " => ";
                                I.replaceAllUsesWith(RHS);
                                errs() << *RHS << "\n";
                            }
                        }
                    }
                }
            }

            return PreservedAnalyses::all();
        }

    // Without isRequired returning true, this pass will be skipped for functions
    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
    // all functions with optnone.
    static bool isRequired() { return true; }
    };
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getPrimoPassoPluginInfo() {
return {LLVM_PLUGIN_API_VERSION, "PrimoPasso", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>) {
                if (Name == "primo-passo") {
                    FPM.addPass(PrimoPasso());
                    return true;
                }
                return false;
                });
        }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize PrimoPasso when added to the pass pipeline on the
// command line, i.e. via '-passes=primo-passo'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getPrimoPassoPluginInfo();
}