#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
using namespace llvm;

//-----------------------------------------------------------------------------
// MultiInstruction implementation
//-----------------------------------------------------------------------------
namespace {

    // New PM implementation
    struct MultiInstruction : PassInfoMixin<MultiInstruction> {
        // Main entry point, takes IR unit to run the pass on (&F) and the corresponding pass manager
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

            bool changed = false;

            for (BasicBlock &BB : F) {
                for (auto &I : BB) {     
                    
                    SmallVector<Instruction*, 16> usersToDelete;

                    // Si salta se l'istruzione non è un BinaryOperator
                    if (!isa<BinaryOperator>(&I))
                        continue;
                    
                    // Si salta alla prossima istruzione se non è una delle basiche operazioni aritmetiche
                    if(
                        I.getOpcode() != Instruction::Add &&
                        I.getOpcode() != Instruction::Sub &&
                        I.getOpcode() != Instruction::Mul &&
                        I.getOpcode() != Instruction::SDiv &&
                        I.getOpcode() != Instruction::UDiv
                    ){
                        continue;
                    }

                    outs() << "\n------------------------\nIstruzione: " << I << "\n";

                    // Itero sulla lista degli Users di I
                    for (auto *U : I.users()){

                        Instruction *user = dyn_cast<Instruction>(&*U);

                        // Si salta se lo user non è un BinaryOperator
                        if (!isa<BinaryOperator>(&*user))
                            continue;

                        outs()<<"\nUser: " << *user << " -> ";

                        // Verifica se lo user sia l'operazione inversa dell'istruzione corrente I
                        bool inverse =
                        (I.getOpcode() == Instruction::Add &&
                            user->getOpcode() == Instruction::Sub) ||
                        (I.getOpcode() == Instruction::Sub &&
                            user->getOpcode() == Instruction::Add) ||
                        (I.getOpcode() == Instruction::Mul &&
                            user->getOpcode() == Instruction::SDiv) ||
                        (I.getOpcode() == Instruction::SDiv &&
                            user->getOpcode() == Instruction::Mul) ||
                        (I.getOpcode() == Instruction::UDiv &&
                            user->getOpcode() == Instruction::Mul);

                        if(!inverse) {
                            outs() << "Non è l'operazione inversa\n";
                            continue;
                        }

                        // Verifico che il primo operando dello User sia l'istruzione corrente
                        if(user->getOperand(0) != &I)
                            continue;
                        
                        // Estrae le costanti (se sono presenti)
                        ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(1));
                        ConstantInt *C2 = dyn_cast<ConstantInt>(user->getOperand(1));

                        // Controlla che siano costanti
                        if(!C1 || !C2) {
                            outs() << "Manca almeno un secondo operando costante\n";
                            continue;
                        }

                        // Controlla che abbiano lo stesso valore
                        if(C1->getValue() != C2->getValue()) {
                            outs() << "Gli operandi costanti non hanno lo stesso valore\n";
                            continue;
                        }

                        outs() << "Inversa trovata\n";

                        // Recupera il primo operando dell'istruzione corrente
                        Value *b = I.getOperand(0);

                        //sostituiamo gli usi di op con binary_operator
                        user->replaceAllUsesWith(b);
                        usersToDelete.push_back(user);

                    }

                    if (!usersToDelete.empty())
                        changed = true;

                    // Istruzioni User da eliminare
                    for (auto *user : usersToDelete) {
                        user->eraseFromParent();
                    }
                }
            }

            return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }

        // Senza questa funzione, il pass potrebbe essere saltato per le funzioni con l'attributo optnone
        static bool isRequired() { return true; }
    };

} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMultiInstructionPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "MultiInstruction", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "multi-inst") {
                        FPM.addPass(MultiInstruction());
                        return true;
                    }
                    return false;
                });
            }};
}

// Questo è il core dell'interfaccia del plugin. Garantisce che 'opt' riconosca il pass quando aggiunto alla pipeline di passaggi sulla riga di comando, ad esempio con '-passes=terzo-passo'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getMultiInstructionPluginInfo();
}