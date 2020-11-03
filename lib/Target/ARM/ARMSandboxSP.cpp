#include "ARM.h"
#include "ARMSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "arm-testpass"

namespace llvm {
void initializeARMSandboxSPPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM sandbox sp"

namespace {
  struct ARMSandboxSP : public MachineFunctionPass{
    static char ID;
    ARMSandboxSP() : MachineFunctionPass(ID) {
      initializeARMSandboxSPPass(*PassRegistry::getPassRegistry());
    }

    const TargetInstrInfo *TII;
    const ARMSubtarget *STI;
    MachineRegisterInfo *MRI;
    const MachineModuleInfo *MMI;

    int NonConstantCount = 0;
    int ConstantSuccessCount = 0;
    int ConstantFailCount = 0;

    bool searchForMemAccess(DenseMap<MachineBasicBlock*, bool> &MBBInfo,
                            MachineBasicBlock *MBB,
                            MachineBasicBlock::iterator MBBI, int imm,
                            int Indent = 0);
    bool runOnMachineFunction(MachineFunction &Fn) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      MachineFunctionPass::getAnalysisUsage(AU);
      AU.addRequired<MachineModuleInfo>();
      AU.addPreserved<MachineModuleInfo>();
    }

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMSandboxSP::ID = 0;
}

INITIALIZE_PASS(ARMSandboxSP, "arm-sandbox-sp", ARM_TESTPASS_NAME, false, false)

static bool isLoadStoreMultiple(unsigned Opcode) {
  switch (Opcode) {
  case ARM::t2STR_PRE:
  case ARM::t2STMDB_UPD:
  case ARM::t2LDMIA_UPD:
    return true;
  default:
    return false;
  }
}

#define PRINT_INDENT for (int i = 0; i < Indent; i++) dbgs() << " "
#define RET_FOUND     do {                                              \
    MBBInfo[MBB] = true;                                                \
    PRINT_INDENT; dbgs() << "  Found it!!\n";                           \
    return true;                                                        \
  } while (0)
#define RET_NOT_FOUND do {                                              \
    MBBInfo[MBB] = false;                                               \
    PRINT_INDENT; dbgs() << "  Not found t.t\n";                        \
    return false;                                                       \
  } while (0)

bool ARMSandboxSP::searchForMemAccess(DenseMap<MachineBasicBlock*, bool> &MBBInfo,
                                      MachineBasicBlock *MBB,
                                      MachineBasicBlock::iterator MBBI, int imm,
                                      int Indent) {
  if (MBBInfo.count(MBB))
    return MBBInfo[MBB];

  MBBInfo[MBB] = true; // in case of a loop

  MachineBasicBlock::iterator MBBIE = MBB->end();
  while (MBBI != MBBIE) {
    MachineInstr *MI = &*MBBI;
    //PRINT_INDENT; MI->dump();
    switch (MI->getOpcode()) {
    case ARM::t2LDRi12: case ARM::t2LDRHi12: case ARM::t2LDRBi12:
    case ARM::t2LDRSHi12: case ARM::t2LDRSBi12:
    case ARM::t2STRi12: case ARM::t2STRHi12: case ARM::t2STRBi12:
    case ARM::t2LDRi8: case ARM::t2LDRHi8: case ARM::t2LDRBi8:
    case ARM::t2LDRSHi8: case ARM::t2LDRSBi8:
    case ARM::t2LDR_PRE: case ARM::t2LDR_POST:
    case ARM::t2LDRH_PRE: case ARM::t2LDRH_POST:
    case ARM::t2LDRB_PRE: case ARM::t2LDRB_POST:
    case ARM::t2LDRSH_PRE: case ARM::t2LDRSH_POST:
    case ARM::t2LDRSB_PRE: case ARM::t2LDRSB_POST:
    case ARM::t2LDMIA_UPD:
    case ARM::t2STRi8: case ARM::t2STRHi8: case ARM::t2STRBi8:
    case ARM::t2STR_PRE: case ARM::t2STR_POST:
    case ARM::t2STRH_PRE: case ARM::t2STRH_POST:
    case ARM::t2STRB_PRE: case ARM::t2STRB_POST:
    case ARM::t2STMDB_UPD:{
      if (MI->getOperand(1).getReg()==ARM::SP)
        RET_FOUND;
      break;
    }
    case ARM::t2LDRDi8: case ARM::t2STRDi8: {
      if (MI->getOperand(2).getReg()==ARM::SP)
        RET_FOUND;
      break;
    }
    case ARM::tLDRspi: case ARM::tSTRspi:
    case ARM::tPUSH: case ARM::tPOP: case ARM::tPOP_RET:
      RET_FOUND;
    case ARM::tBX_RET: case ARM::tBLXr: case ARM::tBX:
      RET_NOT_FOUND;
    case ARM::tBL: {
      MachineOperand &MO = MI->getOperand(2);
      assert(MO.isSymbol() || MO.isGlobal());
      if (MO.isGlobal()) {
        MachineFunction *MF = MMI->getMachineFunction(*cast<Function>(MO.getGlobal()));
        if (MF) {
          bool ret = searchForMemAccess(MBBInfo, &*MF->begin(), MF->begin()->begin(), imm, Indent+1);
          MBBInfo[MBB] = ret;
          return ret;
        }
      }
      RET_NOT_FOUND;
    }
    case ARM::tB: case ARM::t2B: {
      MachineBasicBlock *MBB2 = MI->getOperand(0).getMBB();
      bool ret = searchForMemAccess(MBBInfo, MBB2, MBB2->begin(), imm, Indent+1);
      MBBInfo[MBB] = ret;
      return ret;
    }
    case ARM::tBcc: case ARM::t2Bcc: {
      MachineBasicBlock *MBB2 = MI->getOperand(0).getMBB();
      if (!searchForMemAccess(MBBInfo, MBB2, MBB2->begin(), imm, Indent+1))
        RET_NOT_FOUND;
      break;
    }
    case ARM::tCBZ:
    case ARM::tCBNZ: {
      MachineBasicBlock *MBB2 = MI->getOperand(1).getMBB();
      if (!searchForMemAccess(MBBInfo, MBB2, MBB2->begin(), imm, Indent+1))
        RET_NOT_FOUND;
      break;
    }
    case ARM::tADDspi: {
      if (imm && MI->getOperand(2).getImm() == imm)
        RET_FOUND;
      break;
    }
    default:;
    }
    ++MBBI;
  }

  MachineBasicBlock *FallThrough = MBB->getFallThrough();
  if (!FallThrough) // TODO: handle jump table
    RET_NOT_FOUND;
  bool ret = searchForMemAccess(MBBInfo, FallThrough, FallThrough->begin(), imm, Indent+1);
  MBBInfo[MBB] = ret;
  return ret;
}

bool ARMSandboxSP::runOnMachineFunction(MachineFunction &Fn) {
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  MRI = &Fn.getRegInfo();
  MMI = &getAnalysis<MachineModuleInfo>();

  NonConstantCount = 0;
  ConstantFailCount = 0;
  ConstantSuccessCount = 0;

  bool Modified = false;

  for (MachineBasicBlock &MFI : Fn) {
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      MBBI = NMBBI;
      if (MI.isPseudo())
        continue;

      MachineOperand &MO = MI.getOperand(0);
      if (!MO.isReg() || MO.getReg() != ARM::SP || !MO.isDef())
        continue;

      MachineOperand &MO2 = MI.getOperand(1);
      if (!MO2.isReg())
        continue;

      if (isLoadStoreMultiple(MI.getOpcode())
          || MI.getOpcode() == ARM::t2MOVTi16)
        continue;

      MachineBasicBlock::iterator TmpMBBI = &MI;
      if (MO2.getReg() != ARM::SP) {
        ++TmpMBBI;
        bool ForPrivAccess = false;
        while (TmpMBBI != MBBIE) {
          MachineInstr *TmpMI = &*TmpMBBI;
          if (TmpMI->getOpcode() == ARM::tCPSI) {
            ForPrivAccess = true;
            break;
          }
          ++TmpMBBI;
        }
        if (ForPrivAccess)
          continue;
        dbgs() << "Non-constant SP modification: "
               << TII->getName(MI.getOpcode()) << "\n";
        ++NonConstantCount;
        // TODO: fix this into compare-based
        BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2BICri), ARM::SP)
          .addReg(ARM::SP).addImm(0xd0000000).add(predOps(ARMCC::AL)).add(condCodeOp());
        BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2ORRri), ARM::SP)
          .addReg(ARM::SP).addImm(0x20000000).add(predOps(ARMCC::AL)).add(condCodeOp());
      } else {
        MachineOperand &MO3 = MI.getOperand(2);
        assert(MO3.isImm());
        dbgs() << "Constant SP modification: "; MI.dump();
        ++TmpMBBI;
        int imm = (MI.getOpcode()==ARM::tSUBspi) ? MO3.getImm() : 0;
        DenseMap<MachineBasicBlock*, bool> MBBInfo;
        if (!searchForMemAccess(MBBInfo, &MFI, TmpMBBI, imm)) {
          dbgs() << "Memory Access Search FAILed\n";
          ++ConstantFailCount;
          // TODO: do this correctly
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2BICri), ARM::SP)
            .addReg(ARM::SP).addImm(0xd0000000).add(predOps(ARMCC::AL)).add(condCodeOp());
          // assert(MI.getOpcode()==ARM::tSUBspi || MI.getOpcode()==ARM::t2SUBri
          //        || MI.getOpcode()==ARM::tADDspi || MI.getOpcode()==ARM::t2ADDri);
          // if ((MI.getOpcode()==ARM::tSUBspi || MI.getOpcode()==ARM::t2SUBri)
          //   && MFI.isLiveIn(ARM::R3))
          //   BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2LDRi12), ARM::R12)
          //     .addReg(ARM::SP).addImm(0).add(predOps(ARMCC::AL));
          // else
          //   BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::tLDRspi), ARM::R3)
          //     .addReg(ARM::SP).addImm(0).add(predOps(ARMCC::AL));
        } else {
          dbgs() << "Memory Access Search SUCCESS\n";
          ++ConstantSuccessCount;
        }
      }
    }
  }

  dbgs() << "NonConstantCount        : " << NonConstantCount  << "\n";
  dbgs() << "ConstantFailCount       : " << ConstantFailCount  << "\n";
  dbgs() << "ConstantSuccessCount    : " << ConstantSuccessCount  << "\n";
  return Modified;
}

FunctionPass *llvm::createARMSandboxSPPass() {
  return new ARMSandboxSP();
}
