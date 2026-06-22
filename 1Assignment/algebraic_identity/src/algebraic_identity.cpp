#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// AlgebraicIdentity implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.

/*
**** SPIEGAZIONE ALGORITMO ****

1. Iterare su ogni istruzione (Ottimizzazione locale)
Parto dalla funzione e itero su ogni Basic Block,
e poi per ogni Basic Block itero su ogni istruzione.

2. Analizzare solo le mul o le add
Per ogni istruzione verifico se sia un'istruzione binaria di tipo 'Add' o 'Mul'
In caso positivo procedo, altrimenti continuo con l'iterazione

3. Add o Mul
In entrambi i casi mi salvo il primo e il secondo operando dell'istruzione

3.1 Add
Nel caso della Add, verifico se il primo o il secondo operando sono una costante intera uguale a 0.
Se è così, sostituisco tutti gli usi dell'istruzione corrente direttamente con il Value dell'altro operando

3.2 Mul
Stesso discordo del punto 3.1, con l'eccezione che si deve controllare se uno dei due operandi è una costante intera uguale a 1

Due casi che potrebbero comparire a seguito di front-end poco ottimizzati o a precedenti applicazioni di altri passi di ottimizzazione
sono i seguenti:
- add nsw i32 0, 0
- mul nsw i32 1, 1

Siccome l'algoritmo prevede di verificare prima uno e poi l'altro operando per stabilire in che posizione si trova la costante
(per eseguire poi il rimpiazzamento degli usi dell'istruzione corrente),
la modalità corretta per gestire questi due casi è di evitare che, a seguito del rilevamento della costante 0/1 in una certa posizione,
si faccia poi lo stesso confronto anche sull'altro operando.
Altrimenti il rischio è quello di applicare la stessa sostituzione degli usi due volte.

Esempio:

%2 = add nsw i32 0, 0  ; a
%3 = add nsw i32 0, x0  ; b
%6 = add nsw i32 %2, %3

Diventa

%2 = add nsw i32 0, 0
%3 = add nsw i32 0, %0
%6 = add nsw i32 0, %0

In questo modo ottengo il risultato sperato, ma la sostituzione in %6 di 0 verrebbe eseguita due volte anziché una sola.
Difatti l'output in console sarebbe il seguente:

Algebraic Identity:   %2 = add nsw i32 0, 0 => i32 0
Algebraic Identity:   %2 = add nsw i32 0, 0 => i32 0

*/

namespace {

    // New PM implementation
    struct AlgebraicIdentity: PassInfoMixin<AlgebraicIdentity> {
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

            bool hasReplaced = false;

            for (auto &BB : F) {
                for (auto &I : BB) {

                    if (!isa<BinaryOperator>(&I)) {
                        continue;
                    }

                    Value *LHS = I.getOperand(0);
                    Value *RHS = I.getOperand(1);

                    // Controllo se sia una Add
                    if (I.getOpcode() == Instruction::Add) {

                        // x + 0 => x
                        if (ConstantInt *C_RHS = dyn_cast<ConstantInt>(RHS); C_RHS && C_RHS->isZero())
                        {
                            errs() << "Algebraic Identity: " << I << " => ";
                            I.replaceAllUsesWith(LHS);
                            errs() << *LHS << "\n";
                            hasReplaced = true;
                        }
                        // 0 + x => x
                        else if (ConstantInt *C_LHS = dyn_cast<ConstantInt>(LHS); C_LHS && C_LHS->isZero())
                        {
                            errs() << "Algebraic Identity: " << I << " => ";
                            I.replaceAllUsesWith(RHS);
                            errs() << *RHS << "\n";
                            hasReplaced = true;                           
                        }
                    }

                    // Controllo se sia una Mul
                    else if (I.getOpcode() == Instruction::Mul) {

                        // x * 1 => x
                        if (ConstantInt *C_RHS = dyn_cast<ConstantInt>(RHS); C_RHS && C_RHS->isOne())
                        {
                            errs() << "Algebraic Identity: " << I << " => ";
                            I.replaceAllUsesWith(LHS);
                            errs() << *LHS << "\n";
                            hasReplaced = true;
                        }
                        // 1 * x => x
                        else if (ConstantInt *C_LHS = dyn_cast<ConstantInt>(LHS); C_LHS && C_LHS->isOne())
                        {
                            errs() << "Algebraic Identity: " << I << " => ";
                            I.replaceAllUsesWith(RHS);
                            errs() << *RHS << "\n";
                            hasReplaced = true;
                        }
                    }

                }
            }

            return hasReplaced ? PreservedAnalyses::none() : PreservedAnalyses::all();
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
llvm::PassPluginLibraryInfo getAlgebraicIdentityPluginInfo() {
return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentity", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>) {
                if (Name == "algebraic-identity") {
                    FPM.addPass(AlgebraicIdentity());
                    return true;
                }
                return false;
                });
        }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize AlgebraicIdentity when added to the pass pipeline on the
// command line, i.e. via '-passes=algebraic-identity'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getAlgebraicIdentityPluginInfo();
}