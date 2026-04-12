#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/**
 * Implementazione della Strength Reduction Optimization per le seguenti istruzioni: 
 *   - Moltiplicazione
 *   - Divisione
 * 
 * La strength reduction viene applicata alle istruzioni mul e div che dispongono di un solo operando costante, non entrambi.
 * 
 * Il caso base si verifica quando la costante è una potenza di due, in tal caso basterà sostituire la mul con una lshift e la div con una rigth shift.
 * 
 * x * K => x << n; K = 2^n
 * x / K => x >> n; K = 2^n
 * 
 * Nel caso in cui la costante di una mul non sia esattamente una potenza di due, ma un valore che lo precede o lo segue di una o di due posizioni, 
 * si procede nel seguente modo:
 * - Se K = 2^n - 1 => x * K = (x << n) - x
 * - Se K = 2^n + 1 => x * K = (x << n) + x
 * - Se K = 2^n - 2 => x * K = (x << n) - (x + x)  
 * - Se K = 2^n + 2 => x * K = (x << n) + (x + x)    
 * 
 * Le mul tendenzialmente vengono eseguite in un tempo che varia dai 3-5 cicli di clock. 
 * Purtroppo, operando nel middle end, non si ha accesso a strumenti per compiere ottimizzazioni dipendenti dall'architettura.
 * Per questa versione di S.R. si considera che il numero di cicli di clock per le mul sono pari a 4.
 */

namespace {

  struct StrengthReduction: PassInfoMixin<StrengthReduction> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

      // Analisi

      SmallVector<Instruction*, 16> instToErase;

      for (auto &B : F) {
        for (auto &I : B) {

          // === MOLTIPLICAZIONE ===
          if (I.getOpcode() == Instruction::Mul) {

            ConstantInt *op2 = dyn_cast<ConstantInt>(I.getOperand(1));
            if (!op2) continue;

            APInt K = op2->getValue();

            /*
            Per verificare i 4 possibili casi successivi, dobbiamo utilizzare la variabile K per i confronti.
            K però non è un intero primitivo, bensì un APInt, ovvero un generico intero che può avere diverse rappresentazioni in memoria.
            Poiché l'IR di LLVM fa sì che ogni operazione sia strettamente tipizzata

            Se K dunque è un i32, bitWidth sarù uguale a 32
            */
            unsigned bitWidth = K.getBitWidth();

            // K è una potenza di 2
            if (auto exp = K.exactLogBase2(); exp != -1) {

              BinaryOperator *lshift = BinaryOperator::Create(
                Instruction::Shl,
                I.getOperand(0),
                ConstantInt::get(op2->getIntegerType(), exp)
              );

              lshift->insertBefore(&I);
              I.replaceAllUsesWith(lshift);
              instToErase.push_back(&I);

            } else {

              // Calcolo la potenza di 2 più vicina
              unsigned expNear = K.nearestLogBase2();

              /*
              Esempio con bitWidth = 8, K = 17 e expNear = 4
              APInt(bitWidth, 1) => 0000 0001
              powerOf2 = 0000 0001 << 0000 0100 = 1 * 2^4 = 1 * 16   
              */
              APInt powerOf2 = APInt(bitWidth, 1) << expNear;

              BinaryOperator *lshift = BinaryOperator::Create(
                Instruction::Shl,
                I.getOperand(0),
                ConstantInt::get(op2->getIntegerType(), expNear)
              );

              BinaryOperator *result = NULL;
              BinaryOperator *sadd = NULL;

              if (K == powerOf2 + 1) {
                // K = 2^n + 1 => (x << n) + x
                lshift->insertBefore(&I);
                result = BinaryOperator::Create(Instruction::Add, lshift, I.getOperand(0));

              } else if (K == powerOf2 - 1) {
                // K = 2^n - 1 => (x << n) - x
                lshift->insertBefore(&I);
                result = BinaryOperator::Create(Instruction::Sub, lshift, I.getOperand(0));

              } else if (K == powerOf2 + 2) {
                // K = 2^n + 2 => (x << n) + (x + x)
                lshift->insertBefore(&I);
                sadd = BinaryOperator::Create(Instruction::Add, I.getOperand(0), I.getOperand(0));
                sadd->insertBefore(&I);
                result = BinaryOperator::Create(Instruction::Add, lshift, sadd);

              } else if (K == powerOf2 - 2) {
                // K = 2^n - 2 => (x << n) - (x + x)
                lshift->insertBefore(&I);
                sadd = BinaryOperator::Create(Instruction::Add, I.getOperand(0), I.getOperand(0));
                sadd->insertBefore(&I);
                result = BinaryOperator::Create(Instruction::Sub, lshift, sadd);

              } else {
                delete lshift;
                continue;
              }

              result->insertBefore(&I);
              I.replaceAllUsesWith(result);
              instToErase.push_back(&I);
            }

          // Divisione Unsigned
          } else if (I.getOpcode() == Instruction::UDiv) {

            ConstantInt *op2 = dyn_cast<ConstantInt>(I.getOperand(1));
            if (!op2) continue;

            if (auto exp = op2->getValue().exactLogBase2(); exp != -1) {

              BinaryOperator *rshift = BinaryOperator::Create(
                Instruction::LShr,
                I.getOperand(0),
                ConstantInt::get(op2->getIntegerType(), exp)
              );

              rshift->insertBefore(&I);
              I.replaceAllUsesWith(rshift);
              instToErase.push_back(&I);
            }

          // Divisione signed
          } else if (I.getOpcode() == Instruction::SDiv) {

            ConstantInt *op2 = dyn_cast<ConstantInt>(I.getOperand(1));
            if (!op2) continue;

            if (auto exp = op2->getValue().exactLogBase2(); exp != -1) {
              BinaryOperator *rshift = BinaryOperator::Create(
                Instruction::AShr,
                I.getOperand(0),
                ConstantInt::get(op2->getIntegerType(), exp));

              rshift->insertBefore(&I);
              I.replaceAllUsesWith(rshift);
              instToErase.push_back(&I);
            }
          }
        }
      }

      // Trasformazione

      // Elimino le istruzioni
      for (auto *inst : instToErase)
        inst->eraseFromParent();

      if (!instToErase.empty())
        return PreservedAnalyses::none();
      else
        return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
  };

} // namespace

llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StrengthReduction", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "str-red") {
                    FPM.addPass(StrengthReduction());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getStrengthReductionPluginInfo();
}