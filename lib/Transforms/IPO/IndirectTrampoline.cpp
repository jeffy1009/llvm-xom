#define DEBUG_TYPE "testpass"

#include "llvm/Pass.h"
//#include "llvm/PassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/SmallVector.h"
//#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

cl::opt<bool>
IndirectTrampoline("enable-indirect-trampoline", cl::Hidden, cl::ZeroOrMore,
              cl::desc("Enable indirect trampoline"),
              cl::init(false));

namespace llvm {
  ModulePass *createIndirectTrampolinePass();
  class IndirectTrampolinePass : public ModulePass {

  public:
    static char ID;

    IndirectTrampolinePass() : ModulePass(ID) {
      initializeIndirectTrampolinePassPass(*PassRegistry::getPassRegistry());
    };

    bool runOnModule(Module &M) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override;
  };
}

char IndirectTrampolinePass::ID = 0;

ModulePass *llvm::createIndirectTrampolinePass() {
  return new IndirectTrampolinePass();
}

static void addIndirectTrampolinePass(const PassManagerBuilder &builder,
			      legacy::PassManagerBase &PM) {
  PM.add(new IndirectTrampolinePass());
}

// set pass
// static RegisterStandardPasses s(PassManagerBuilder::EP_OptimizerLast,
// 				addIndirectTrampolinePass);

INITIALIZE_PASS(IndirectTrampolinePass, "indirect-trampoline-pass", "IndirectTrampoline Pass", false, false)

bool handleInstruction(User *FU) {
  dbgs() << "handleInstruction :"; FU->dump();
  assert(isa<StoreInst>(FU) || isa<CallInst>(FU) || isa<CmpInst>(FU) || isa<SelectInst>(FU) || isa<PHINode>(FU));
  if (isa<StoreInst>(FU) || isa<CmpInst>(FU) || isa<SelectInst>(FU) || isa<PHINode>(FU)) {
    dbgs() << "JSSHIN  NeedTramp store inst: ";
    FU->dump();
    return true;
  }

  if (CallInst *CI = dyn_cast<CallInst>(FU)) {
    if (CI->getCalledFunction() != FU) {
      dbgs() << "JSSHIN  NeedTramp call inst: ";
      FU->dump();
      return true;
    }
  }

  return false;
}

bool handleGlobalVariable(GlobalVariable *GV) {
  if (!GV->getName().equals("llvm.used")) {
    dbgs() << "JSSHIN    NeedTramp. global value: ";
    GV->dump();
    return true;
  }
  return false;
}

bool IndirectTrampolinePass::runOnModule(Module &M) {
  if (!IndirectTrampoline)
    return false;

  srand((unsigned)time(NULL));
  LLVMContext &Context = M.getContext();

  SmallVector<Function *, 32> Functions;
  for (Function &F : M.functions()) {
    if (F.use_empty())
      continue;

    if (!F.hasAddressTaken())
      continue;
    Functions.push_back(&F);    
  }

  while (!Functions.empty()) {
    Function *F = Functions.back();
    Functions.pop_back();
    bool NeedTramp = false;

    // start handling annoying cases..
    for ( Use &U : F->uses()) {
      User *FU = U.getUser();

      if (isa<Instruction>(FU)) {
	if (handleInstruction(FU)) {
	  NeedTramp = true;
	  break;
	}
	continue;
      }

      if (isa<Constant>(FU)) {
	if (isa<BlockAddress>(FU))
	  continue;

	if (isa<GlobalValue>(FU)) {
	  dbgs() << "JSSHIN GlobalValue: "; FU->dump();
	  assert(isa<GlobalAlias>(FU) || isa<GlobalVariable>(FU));
	  if (isa<GlobalAlias>(FU))
	    continue;
	  if (handleGlobalVariable(cast<GlobalVariable>(FU))) {
	    NeedTramp = true;
	    break;
	  }
	  continue;
	}

	if (isa<ConstantAggregate>(FU)) {
	  for (User *U2 : FU->users()) {
	    assert(isa<ConstantAggregate>(U2) || isa<GlobalVariable>(U2));
	    if (isa<ConstantAggregate>(U2)) {
	      for (User *U3 : U2->users()) {
		assert(isa<GlobalVariable>(U3));
		if (handleGlobalVariable(cast<GlobalVariable>(U3))) {
		  NeedTramp = true;
		  break;
		}
		continue;
	      }
	      continue;
	    }

	    if (handleGlobalVariable(cast<GlobalVariable>(U2))) {
	      NeedTramp = true;
	      break;
	    }
	    continue;
	  }
	  continue;
	}

	assert(isa<ConstantExpr>(FU));
	dbgs() << "JSSHIN  Constant expr: ";
	FU->dump();

	for (User *U2 : FU->users()) {
	  if (isa<Instruction>(U2)) {
	    if (handleInstruction(U2)) {
	      NeedTramp = true;
	      break;
	    }
	    continue;
	  }

	  if (isa<Constant>(U2)) {
	    if (GlobalValue *GV = dyn_cast<GlobalValue>(U2)) {
	      if (!GV->getName().equals("llvm.used")) {
		dbgs() << "JSSHIN    NeedTramp. global value: ";
		GV->dump();
		NeedTramp = true;
		break;
	      }
	      dbgs() << "JSSHIN    global value: ";
	      U2->dump();
	      continue;
	    }

	    assert(isa<ConstantAggregate>(U2)||isa<ConstantExpr>(U2));
	    for (User *U3 : U2->users()) {
	      assert(!isa<Instruction>(U3));

	      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(U3)) {
		if (!GV->getName().equals("llvm.used")) {
		  dbgs() << "JSSHIN    NeedTramp. global value: ";
		  GV->dump();
		  NeedTramp = true;
		  break;
		}
		continue;
	      }
	      assert(!isa<Constant>(U3));
	      assert(0);
	    }
	    if (NeedTramp)
	      break;
	    continue;
	  }
	  assert(0 && "JSSHIN Unreacheable!");
	}

	if (NeedTramp)
	  break;

	continue;
      }
      assert(0 && "JSSHIN Unreachable!");
    } // end for ( Use &U : F->uses())
    // end handling annoying cases..

    if (!NeedTramp)
      continue;

    dbgs() << "JSSHIN  Creating trampoline for: " << F->getName() << '\n';

    SmallVector<Use *, 32> Uses;
    for ( Use &U : F->uses())
      Uses.push_back(&U);

    Function *foo = F;
    Type *fooType = foo->getReturnType();

    // create foo_trampoline function
    SmallString<16> TrampName;
    SmallString<8> IDName;

    // to avoid same function name
    char ID = std::abs(rand()) % 1000;
    Twine fooName = F->getName() + std::to_string(ID) + "_Tramp";
    Function *fooTramp = cast<Function>(M.getOrInsertFunction(fooName.toStringRef(TrampName),
							      cast<FunctionType>(foo->getType()->getElementType()),
							      foo->getAttributes()));
    fooTramp->setLinkage(GlobalValue::InternalLinkage);
    for (Use *U : Uses) {
      User *Usr = U->getUser();
      int location;
      for (location = 0; location < Usr->getNumOperands() - 1; location++) {
      	if (Usr->getOperand(location)->getName() == F->getName())
      	  break;
      }

      SmallVector<Value *, 8> params;
      SmallVector<Type *, 8> paramTypes;

      // initializing new argument by calling args iterator
      for (Argument &i : fooTramp->args()) { 
      	params.push_back(&i);
	paramTypes.push_back(i.getType());
      }
      ArrayRef<Value *> Args(params);
      Function::arg_iterator ArgI = foo->arg_begin();
      BasicBlock *BBTramp = BasicBlock::Create(Context, "", fooTramp);
      
      // add call instruction in foo_Tramp
      IRBuilder<> Builder(BBTramp);
      CallInst *CI = Builder.CreateCall(foo, Args);
      CI->setTailCall();

      // determine return type
      if (fooType->isVoidTy())
	Builder.CreateRetVoid();
      else
	Builder.CreateRet(CI);

      int location2;
      for (location2 = 0; location2 < Usr->getNumOperands() - 1; location2++) {
	if (Usr->getOperand(location2)->getName() == F->getName())
	  break;
      }
      Usr->setOperand(location2, fooTramp);
    }  // endfor (Use *U : Uses)
  } // endwhile (!Functions.empty())
  return true;
}

void IndirectTrampolinePass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}
