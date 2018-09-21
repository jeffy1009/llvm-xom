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
// #include "llvm/CodeGen/MachineRegisterInfo.h"
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
void initializeARMTestPassPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME                                       \
  "ARM test pass"

namespace {
  struct ARMTestPass : public MachineFunctionPass{
    static char ID;
    ARMTestPass() : MachineFunctionPass(ID) {
      initializeARMTestPassPass(*PassRegistry::getPassRegistry());
    }

    // const DataLayout *TD;
    const TargetInstrInfo *TII;
    // const TargetRegisterInfo *TRI;
    const ARMSubtarget *STI;
    // MachineRegisterInfo *MRI;
    // MachineFunction *MF;

	int count = 0;

    bool runOnMachineFunction(MachineFunction &Fn) override;

    const char *getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMTestPass::ID = 0;
}

INITIALIZE_PASS(ARMTestPass, "arm-testpass",
                ARM_TESTPASS_NAME, false, false)

/// Returns true if instruction is a memory operation that this pass is capable
/// of operating on.
static bool isMemoryOp(const MachineInstr &MI) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  case ARM::tLDRBi:

	//[TODO] check opcodes
    //      case ARM::VLDRS:
    //      case ARM::VLDRD:
    
    //  case ARM::LDRi12:
    //  case ARM::tLDRi:
    /*
  case ARM::tLDRspi:
  case ARM::t2LDRi8:
  case ARM::t2LDRi12:

	case ARM::LDRH:
	case ARM::tLDRHi:
	case ARM::t2LDRHi8:
	case ARM::t2LDRHi12:

	case ARM::LDRBi12:
	case ARM::tLDRBi:
	case ARM::t2LDRBi8:
	case ARM::t2LDRBi12:

	case ARM::LDRSH:
	case ARM::tLDRSH:
	case ARM::t2LDRSHi8:
	case ARM::t2LDRSHi12:

	case ARM::LDRSB:
	case ARM::tLDRSB:
	case ARM::t2LDRSBi8:
	case ARM::t2LDRSBi12:

	case ARM::LDRrs:
	case ARM::tLDRr:
	case ARM::t2LDRs:

	case ARM::t2LDRHs:
	case ARM::tLDRHr:

	case ARM::t2LDRBs:
	case ARM::tLDRBr:

	case ARM::t2LDRSHs:

	case ARM::t2LDRSBs:

	//[TODO] ldm is ommitted
	case ARM::LDRD:
	case ARM::t2LDRDi8:
    */
    break;
  default:
    return false;
  }
/*
  if (!MI.getOperand(1).isReg())
    return false;

  // When no memory operands are present, conservatively assume unaligned,
  // volatile, unfoldable.
  if (!MI.hasOneMemOperand())
    return false;

  const MachineMemOperand &MMO = **MI.memoperands_begin();

  // Don't touch volatile memory accesses - we may be changing their order.
  if (MMO.isVolatile())
    return false;

  // Unaligned ldr/str is emulated by some kernels, but unaligned ldm/stm is
  // not.
  if (MMO.getAlignment() < 4)
    return false;

  // str <undef> could probably be eliminated entirely, but for now we just want
  // to avoid making a mess of it.
  // FIXME: Use str <undef> as a wildcard to enable better stm folding.
  if (MI.getOperand(0).isReg() && MI.getOperand(0).isUndef())
    return false;

  // Likewise don't mess with references to undefined addresses.
  if (MI.getOperand(1).isUndef())
    return false;
*/
  return true;
}

//bool is

bool ARMTestPass::runOnMachineFunction(MachineFunction &Fn) {
  // TD = &Fn.getDataLayout();
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  // TRI = STI->getRegisterInfo();
  // MRI = &Fn.getRegInfo();
  // MF  = &Fn;



  bool Modified = false;

  for (MachineBasicBlock &MFI : Fn) {
    //    if(Fn.getName().compare(StringRef("sort")) != 0){
    //      dbgs() << "Current function name: [" << Fn.getName() << "]\n";
    //      continue;
    //    }

    
    for (MachineInstr &MI : MFI) {
      if (!isMemoryOp(MI)) continue;
	count++;
        dbgs() << "instrumented !!!!! : " << count  << "\n";

        unsigned Reg1 = MI.getOperand(0).getReg();
        unsigned Reg2 = MI.getOperand(1).getReg();
        unsigned Imm = MI.getOperand(2).getImm();
        Imm = Imm << 2;
        //        unsigned Imm = 0;
        AddDefaultPred(BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::t2LDRBT))
                       .addReg(Reg1)
                       .addReg(Reg2)
                       .addImm(Imm));
        //   	unsigned Reg = MI.getOperand(1).getReg();
        //	AddDefaultPred(BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::tMOVr))
	//	.addReg(Reg)
	//	.addReg(Reg));

//	AddDefaultPred(BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::tMOVr))
//		.addReg(Reg)
//		.addReg(Reg));
    }
  }

  return Modified;
}

FunctionPass *llvm::createARMTestPassPass() {
  return new ARMTestPass();
}
