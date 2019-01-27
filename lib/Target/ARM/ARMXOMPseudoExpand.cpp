#include "ARM.h"
#include "ARMSubtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
using namespace llvm;

#define DEBUG_TYPE "arm-testpass"

namespace llvm {
void initializeARMXOMPseudoExpandPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM xom pseudo expand pass"

namespace {
  struct ARMXOMPseudoExpand : public MachineFunctionPass{
    static char ID;
    ARMXOMPseudoExpand() : MachineFunctionPass(ID) {
      initializeARMXOMPseudoExpandPass(*PassRegistry::getPassRegistry());
    }

    const TargetInstrInfo *TII;
    const ARMSubtarget *STI;
    MachineRegisterInfo *MRI;

	int count = 0;

    bool runOnMachineFunction(MachineFunction &Fn) override;

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMXOMPseudoExpand::ID = 0;
}

INITIALIZE_PASS(ARMXOMPseudoExpand, "arm-xom-pseudo-expand", ARM_TESTPASS_NAME, false, false)

enum { INST_NONE, INST_ADD_IMM, INST_SUB_IMM, INST_ADD_REG, INST_ADD_REG_SHFT };

bool ARMXOMPseudoExpand::runOnMachineFunction(MachineFunction &Fn) {
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  MRI = &Fn.getRegInfo();

  bool Modified = false;
  if(Fn.getName().compare(StringRef("__sfputc_r")) == 0)
    return Modified;

  for (MachineBasicBlock &MFI : Fn) {
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      switch (MI.getOpcode()) {
      case ARM::XOM_CHECK_ADDR_MPU: {
        BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::t2CMPrr))
          .addReg(ARM::SP).add(MI.getOperand(0)).add(predOps(ARMCC::AL));
        BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::t2IT)).addImm(ARMCC::HS).addImm(0x14);
        BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::t2CMPrr))
          .addReg(ARM::SP).add(MI.getOperand(1)).add(predOps(ARMCC::AL));
        BuildMI(MFI, MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi), ARM::SP)
          .addImm(0).add(predOps(ARMCC::LO)).addReg(ARM::CPSR, RegState::Implicit);
        MI.eraseFromParent();
        break;
      }
      }
      MBBI = NMBBI;
    }
  }

  return Modified;
}

FunctionPass *llvm::createARMXOMPseudoExpandPass() {
  return new ARMXOMPseudoExpand();
}
