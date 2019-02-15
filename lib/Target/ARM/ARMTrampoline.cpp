#include "ARM.h"
// #include "ARMBaseInstrInfo.h"
// #include "ARMBaseRegisterInfo.h"
// #include "ARMISelLowering.h"
// #include "ARMMachineFunctionInfo.h"
#include "ARMSubtarget.h"
// #include "MCTargetDesc/ARMAddressingModes.h"
// #include "ThumbRegisterInfo.h"
// #include "llvm/ADT/DenseMap.h"
// #include "llvm/ADT/STLExtras.h"
// #include "llvm/ADT/SmallPtrSet.h"
// #include "llvm/ADT/SmallSet.h"
// #include "llvm/ADT/SmallVector.h"
// #include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
// #include "llvm/CodeGen/MachineInstr.h"
// #include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
// #include "llvm/CodeGen/RegisterClassInfo.h"
// #include "llvm/CodeGen/SelectionDAGNodes.h"
// #include "llvm/CodeGen/LivePhysRegs.h"
// #include "llvm/IR/DataLayout.h"
// #include "llvm/IR/DerivedTypes.h"
// #include "llvm/IR/Function.h"
// #include "llvm/Support/Allocator.h"
// #include "llvm/Support/Debug.h"
// #include "llvm/Support/ErrorHandling.h"
// #include "llvm/Support/raw_ostream.h"
// #include "llvm/Target/TargetInstrInfo.h"
// #include "llvm/Target/TargetMachine.h"
// #include "llvm/Target/TargetRegisterInfo.h"
using namespace llvm;

#define DEBUG_TYPE "arm-testpass"

namespace llvm {
void initializeARMTrampolinePass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM trampoline pass"

namespace {
  struct ARMTrampoline : public MachineFunctionPass{
    static char ID;
    ARMTrampoline() : MachineFunctionPass(ID) {
      initializeARMTrampolinePass(*PassRegistry::getPassRegistry());
    }

    // const DataLayout *TD;
    const TargetInstrInfo *TII;
    // const TargetRegisterInfo *TRI;
    const ARMSubtarget *STI;
    MachineRegisterInfo *MRI;
    // MachineFunction *MF;

    int count = 0;

    bool runOnMachineFunction(MachineFunction &Fn) override;

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMTrampoline::ID = 0;
}

INITIALIZE_PASS(ARMTrampoline, "arm-mask-lr-reg", ARM_TESTPASS_NAME, false, false)

bool ARMTrampoline::runOnMachineFunction(MachineFunction &Fn) {
  // To avoid IR Tramp function
  StringRef FName = Fn.getName().take_back(5);
  StringRef TName = StringRef("Tramp");
  if (!FName.compare(TName))
    return true;
  
  SmallVector<MachineInstr *, 8> BranchInsts;
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  MRI = &Fn.getRegInfo();

  bool Modified = false;

#define LR_MASK 0x80000000

  for (MachineBasicBlock &MFI : Fn) {
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      switch (MI.getOpcode()) {
      case ARM::tBLXr: {
      	// fall-through
      }
      case ARM::tBL: {
	// do not split basic block during iteration
	BranchInsts.push_back(&MI);
	break;
      }
      default:;
      }

      MBBI = NMBBI;
    }
  }
  
  // gykim
  // for queue
  SmallVector <MachineInstr *, 8> BranchStack;
  while (!BranchInsts.empty()) {
    BranchStack.push_back(BranchInsts.back());
    BranchInsts.pop_back();
  }

  // split basic block
  while (!BranchStack.empty()) {
    MachineInstr *CurMI = BranchStack.back();
    BranchStack.pop_back();
    MachineBasicBlock *CurMBB = CurMI->getParent();
    MachineBasicBlock::iterator CurMII = CurMI->getIterator();
    MachineFunction::iterator CurMBBI = CurMBB->getIterator();
    MachineFunction *CurMF = CurMBB->getParent();
    MachineBasicBlock *NewMBB = CurMF->CreateMachineBasicBlock();
    MachineBasicBlock *NewMBB2 = CurMF->CreateMachineBasicBlock();
    CurMF->push_back(NewMBB);
    CurMF->insert(++CurMBBI, NewMBB2);

    NewMBB->transferSuccessors(CurMBB);
    CurMBB->addSuccessor(NewMBB);

    // insert 'tB NewMBB' before tBL
    STI = &static_cast<const ARMSubtarget &>(CurMF->getSubtarget());
    TII = STI->getInstrInfo();    
    BuildMI(*CurMBB, CurMII, CurMI->getDebugLoc(), TII->get(ARM::tB)).addMBB(NewMBB);

    // move instructions under tB to NewMBB 
    NewMBB->splice(NewMBB->end(), CurMBB, CurMI, CurMBB->end());

    // split once again
    CurMBB = CurMI->getParent();
    CurMII = CurMI->getIterator();
    CurMBBI = CurMBB->getIterator();
    CurMF = CurMBB->getParent();
    NewMBB2->transferSuccessors(CurMBB);
    CurMBB->addSuccessor(NewMBB2);
    BuildMI(*CurMBB, ++CurMII, CurMI->getDebugLoc(), TII->get(ARM::tB)).addMBB(NewMBB2);
    NewMBB2->splice(NewMBB2->end(), CurMBB, CurMII, CurMBB->end());

    dbgs();
}
  
  return Modified;
}

FunctionPass *llvm::createARMTrampolinePass() {
  return new ARMTrampoline();
}
