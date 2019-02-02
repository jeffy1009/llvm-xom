#include "ARM.h"
#include "ARMSubtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
using namespace llvm;

#define DEBUG_TYPE "arm-testpass"

namespace llvm {
void initializeARMBranchSFIPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM branch sfi pass"

namespace {
  struct ARMBranchSFI : public MachineFunctionPass{
    static char ID;
    ARMBranchSFI() : MachineFunctionPass(ID) {
      initializeARMBranchSFIPass(*PassRegistry::getPassRegistry());
    }

    const TargetInstrInfo *TII;
    const ARMSubtarget *STI;
    MachineRegisterInfo *MRI;

    bool runOnMachineFunction(MachineFunction &Fn) override;

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMBranchSFI::ID = 0;
}

INITIALIZE_PASS(ARMBranchSFI, "arm-branch-sfi", ARM_TESTPASS_NAME, false, false)

bool ARMBranchSFI::runOnMachineFunction(MachineFunction &Fn) {
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  MRI = &Fn.getRegInfo();

  // Align externally visible functions at 16Byte boundary
  if (Fn.getFunction()->getLinkage()==GlobalValue::ExternalLinkage)
    Fn.ensureAlignment(4);

  bool Modified = false;
#define SFI_MASK 0x80000000
  for (MachineBasicBlock &MFI : Fn) {
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      switch (MI.getOpcode()) {
      case ARM::tBLXr: {
        unsigned reg = MI.getOperand(2).getReg();
        BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2BICri), reg)
          .addReg(reg).addImm(SFI_MASK).add(predOps(ARMCC::AL)).add(condCodeOp());
        break;
      }
      case ARM::tBX_RET: {
        BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2BICri), ARM::LR)
          .addReg(ARM::LR).addImm(SFI_MASK).add(predOps(ARMCC::AL)).add(condCodeOp());
        break;
      }
      case ARM::tPOP_RET: {
        SmallVector<MachineOperand, 4> ImpUses;
        int OpPC = 0;
        int OpIT = 0;
        for (unsigned i = 2, e = MI.getNumOperands(); i != e; ++i) {
          MachineOperand &MO = MI.getOperand(i);
          if (MO.isImplicit() && MO.isUse())
            ImpUses.push_back(MO);
          if (MO.getReg() == ARM::PC) {
            OpPC = i;
            continue;
          }
          if (MO.getReg() == ARM::ITSTATE) {
            OpIT = i;
            continue;
          }
        }

        // TODO: handle ITSTATE
        if (OpIT > 0)
          break;

        if (OpPC == 0)
          break;

        MachineInstrBuilder MIB =
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2LDMIA_UPD), ARM::SP)
          .addReg(ARM::SP);
        for (int i = 0; i < OpPC; i++)
          MIB.add(MI.getOperand(i));
        MIB.addReg(ARM::LR, RegState::Define);
        BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2BICri), ARM::LR)
          .addReg(ARM::LR).addImm(SFI_MASK).add(predOps(ARMCC::AL)).add(condCodeOp());
        BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::tBX_RET))
          .add(predOps(ARMCC::AL)).add(ImpUses);
        MI.eraseFromParent();
        break;
      }
      default:; // TODO: handle t2BR_JT
      }

      MBBI = NMBBI;
    }
  }

  return Modified;
}

FunctionPass *llvm::createARMBranchSFIPass() {
  return new ARMBranchSFI();
}
