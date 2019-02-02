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

extern cl::opt<bool> EnableXOMSFI;

namespace llvm {
void initializeARMTestPassPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM test pass"

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
    MachineRegisterInfo *MRI;
    // MachineFunction *MF;

    int InstRegCount = 0;
    int InstImmPreIdxCount = 0;
    int InstImmPostIdxCount = 0;
    int InstImmNoIdxOffsetCount = 0;
    int InstImmNoIdxCount = 0;
    int InstDoubleOffsetCount = 0;
    int InstDoubleCount = 0;
    int InstRegPrivCount = 0;
    int InstImmPrivVarCount = 0;
    int InstImmPrivCount = 0;

    // IsVariable: load/store address is not fixed
    bool requirePrivilege(unsigned Reg, MachineInstr *&DefMI, bool &IsVariable);

    void addOffsetInstReg(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                          unsigned dest_reg, MachineOperand *reg1, MachineOperand *reg2,
                          int shift_imm);
    void addOffsetInstImm(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                          unsigned dest_reg, MachineOperand *reg1, int imm);

    void instChecks(MachineBasicBlock &MFI, MachineInstr &MI, MachineInstr *DefMI,
                    unsigned new_opcode, bool isStore);
    void instPostCheck(MachineBasicBlock &MFI, MachineBasicBlock::iterator MI);
    bool instReg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                 MachineBasicBlock &MFI, bool isStore);
    bool instImmSFI(MachineInstr &MI, MachineBasicBlock &MFI, unsigned base_reg,
                    bool isStore);
    bool instImm(unsigned Opcode, unsigned new_opcode,
                 MachineInstr &MI, MachineBasicBlock &MFI, bool isStore);
    bool instLDSTDouble(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                        MachineBasicBlock &MFI, bool isStore);
    bool instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI);

    bool runOnMachineFunction(MachineFunction &Fn) override;

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMTestPass::ID = 0;
}

INITIALIZE_PASS(ARMTestPass, "arm-testpass", ARM_TESTPASS_NAME, false, false)

enum { INST_NONE, INST_ADD_IMM, INST_SUB_IMM, INST_ADD_REG, INST_ADD_REG_SHFT };

static bool isT3Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDRi12: case ARM::t2LDRHi12: case ARM::t2LDRBi12:
  case ARM::t2LDRSHi12: case ARM::t2LDRSBi12:
  case ARM::t2STRi12: case ARM::t2STRHi12: case ARM::t2STRBi12:
    return true;
  default:
    return false;
  }
}

static bool isT4Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDRi8: case ARM::t2LDRHi8: case ARM::t2LDRBi8:
  case ARM::t2LDRSHi8: case ARM::t2LDRSBi8:
  case ARM::t2LDR_PRE: case ARM::t2LDR_POST:
  case ARM::t2LDRH_PRE: case ARM::t2LDRH_POST:
  case ARM::t2LDRB_PRE: case ARM::t2LDRB_POST:
  case ARM::t2LDRSH_PRE: case ARM::t2LDRSH_POST:
  case ARM::t2LDRSB_PRE: case ARM::t2LDRSB_POST:
  case ARM::t2STRi8: case ARM::t2STRHi8: case ARM::t2STRBi8:
  case ARM::t2STR_PRE: case ARM::t2STR_POST:
  case ARM::t2STRH_PRE: case ARM::t2STRH_POST:
  case ARM::t2STRB_PRE: case ARM::t2STRB_POST:
    return true;
  default:
    return false;
  }
}

static bool isT4PreEncoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDR_PRE: case ARM::t2LDRH_PRE: case ARM::t2LDRB_PRE:
  case ARM::t2LDRSH_PRE: case ARM::t2LDRSB_PRE:
  case ARM::t2STR_PRE: case ARM::t2STRH_PRE: case ARM::t2STRB_PRE:
    return true;
  default:
    return false;
  }
}

static bool isT4PostEncoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDR_POST: case ARM::t2LDRH_POST: case ARM::t2LDRB_POST:
  case ARM::t2LDRSH_POST: case ARM::t2LDRSB_POST:
  case ARM::t2STR_POST: case ARM::t2STRH_POST: case ARM::t2STRB_POST:
    return true;
  default:
    return false;
  }
}

#define SPECIAL_REG ARM::SP

bool ARMTestPass::requirePrivilege(unsigned Reg, MachineInstr *&DefMI, bool &IsVariable) {
  // passed as an argument to or returned from a function
  if (TargetRegisterInfo::isPhysicalRegister(Reg))
    return false;
  assert(MRI->hasOneDef(Reg) && "Virtual register has multiple defs?");

  MachineInstr *MI = &*MRI->def_instr_begin(Reg);
  switch (MI->getOpcode()) {
  case ARM::t2MOVi: case ARM::t2MOVi16: case ARM::t2MOVi32imm:
  case ARM::t2MOVCCi: case ARM::t2MOVCCi32imm: case ARM::t2MOVCCr: {
    if (!MI->getOperand(1).isImm() || (unsigned)MI->getOperand(1).getImm() < 0xe0000000)
      break;
    dbgs() << "  Not instrumented, Privilege required.\n";
    DefMI = MI;
    DEBUG(MI->dump());
    return true;
  }
  case TargetOpcode::COPY: {
    if (MI->getOperand(1).getReg()==SPECIAL_REG)
      return false;
    return MI->getOperand(1).isReg() && requirePrivilege(MI->getOperand(1).getReg(), DefMI, IsVariable);
  }
  case ARM::t2ADDri: case ARM::t2ADDrr: case ARM::t2ADDri12: {
    if (MI->getOperand(2).isReg())
      IsVariable = true;
    return MI->getOperand(1).isReg() && requirePrivilege(MI->getOperand(1).getReg(), DefMI, IsVariable);
  }
  }
  return false;
}

void ARMTestPass::
addOffsetInstReg(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                 unsigned dest_reg, MachineOperand *reg1, MachineOperand *reg2,
                 int shift_imm) {
  if(insttype == INST_ADD_REG){
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDrr), dest_reg)
      .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0)
      .addReg(reg2->getReg(), reg2->isKill() ? RegState::Kill : 0)
      .add(predOps(ARMCC::AL)).add(condCodeOp());
  }else if(insttype == INST_ADD_REG_SHFT){
    // add.w rd, rn, rm lsl #imm
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDrs), dest_reg)
      .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0)
      .addReg(reg2->getReg(), reg2->isKill() ? RegState::Kill : 0)
      .addImm(ARM_AM::getSORegOpc(ARM_AM::lsl, shift_imm))
      .add(predOps(ARMCC::AL)).add(condCodeOp());
  }
}

void ARMTestPass::
addOffsetInstImm(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                 unsigned dest_reg, MachineOperand *reg1, int imm) {
  if(insttype == INST_ADD_IMM){
    if (imm < 256)
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDri), dest_reg)
        .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0).addImm(imm)
        .add(predOps(ARMCC::AL)).add(condCodeOp());
    else
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDri12), dest_reg)
        .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0).addImm(imm)
        .add(predOps(ARMCC::AL));
  }else if(insttype == INST_SUB_IMM){
    if (imm < 256)
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2SUBri), dest_reg)
        .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0).addImm(imm)
        .add(predOps(ARMCC::AL)).add(condCodeOp());
    else
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2SUBri12), dest_reg)
        .addReg(reg1->getReg(), reg1->isKill() ? RegState::Kill : 0).addImm(imm)
        .add(predOps(ARMCC::AL));
  }
}

enum { XOM_LD_W, XOM_LD_H, XOM_LD_SH, XOM_LD_B, XOM_LD_SB,
       XOM_ST_W, XOM_ST_H, XOM_ST_B };

static unsigned getPrivilegedOpcode(unsigned Opcode) {
  switch (Opcode) {
  case XOM_LD_W:  return ARM::t2LDRT;
  case XOM_LD_H:  return ARM::t2LDRHT;
  case XOM_LD_SH: return ARM::t2LDRSHT;
  case XOM_LD_B:  return ARM::t2LDRBT;
  case XOM_LD_SB: return ARM::t2LDRSBT;
  case XOM_ST_W:  return ARM::t2STRT;
  case XOM_ST_H:  return ARM::t2STRHT;
  case XOM_ST_B:  return ARM::t2STRBT;
  default: llvm_unreachable("Unhandled opcode");
  }
}

static unsigned getSFIOpcode(unsigned Opcode) {
  switch (Opcode) {
  case XOM_LD_W:  return ARM::t2LDRi12;
  case XOM_LD_H:  return ARM::t2LDRHi12;
  case XOM_LD_SH: return ARM::t2LDRSHi12;
  case XOM_LD_B:  return ARM::t2LDRBi12;
  case XOM_LD_SB: return ARM::t2LDRSBi12;
  case XOM_ST_W:  return ARM::t2STRi12;
  case XOM_ST_H:  return ARM::t2STRHi12;
  case XOM_ST_B:  return ARM::t2STRBi12;
  default: llvm_unreachable("Unhandled opcode");
  }
}

#define SCS_START 0xe0000000
#define LD_BOUNDARY 0x80000
#define ST_BOUNDARY SCS_START

void ARMTestPass::
instChecks(MachineBasicBlock &MFI, MachineInstr &MI, MachineInstr *DefMI,
           unsigned new_opcode, bool isStore) {
#define MPU_START 0xE000ED90
#define MPU_END   0xE000EDEF
#define VTOR_ADDR 0xE000ED08

  if (isStore) {
    unsigned mpu_start = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    unsigned mpu_end = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi32imm), mpu_start)
      .addImm(MPU_START);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi32imm), mpu_end)
      .addImm(MPU_END);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::XOM_CHECK_ADDR_MPU))
      .addReg(mpu_start).addReg(mpu_end);
    unsigned vtor_addr = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi32imm), vtor_addr)
      .addImm(VTOR_ADDR);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPrr))
      .addReg(SPECIAL_REG).addReg(vtor_addr).add(predOps(ARMCC::AL));
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi), SPECIAL_REG)
      .addImm(0).add(predOps(ARMCC::EQ)).addReg(ARM::CPSR, RegState::Implicit);
  } else {
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPri))
      .addReg(SPECIAL_REG).addImm(SCS_START).add(predOps(ARMCC::AL));
    // TODO: currently SP is set to zero if checking fails.
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi), SPECIAL_REG)
      .addImm(0).add(predOps(ARMCC::LO)).addReg(ARM::CPSR, RegState::Implicit);
  }
  BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(getSFIOpcode(new_opcode)))
    .add(MI.getOperand(0))
    .addReg(SPECIAL_REG).addImm(0).add(predOps(ARMCC::AL));
}

void ARMTestPass::instPostCheck(MachineBasicBlock &MFI, MachineBasicBlock::iterator MI) {
  BuildMI(MFI, MI, DebugLoc(), TII->get(ARM::t2CMPri))
    .addReg(SPECIAL_REG).addImm(0x20000000).add(predOps(ARMCC::AL));
  BuildMI(MFI, MI, DebugLoc(), TII->get(ARM::t2MOVi), SPECIAL_REG)
    .addImm(0).add(predOps(ARMCC::LO)).addReg(ARM::CPSR, RegState::Implicit);
  BuildMI(MFI, MI, DebugLoc(), TII->get(ARM::t2CMPri))
    .addReg(SPECIAL_REG).addImm(0x40000000).add(predOps(ARMCC::AL));
  BuildMI(MFI, MI, DebugLoc(), TII->get(ARM::t2MOVi), SPECIAL_REG)
    .addImm(0).add(predOps(ARMCC::HS)).addReg(ARM::CPSR, RegState::Implicit);
}

bool ARMTestPass::
instReg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
        MachineBasicBlock &MFI, bool isStore) {
  MachineOperand *value_MO = &MI.getOperand(0);
  MachineOperand *base_MO = &MI.getOperand(1);
  MachineOperand *offset_MO = &MI.getOperand(2);

  unsigned new_inst;
  int shift_imm;

  // LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}]
  dbgs() << "T2 encoding for " << (isStore? "str" : "ldr") << " (reg)\n";
  shift_imm = MI.getOperand(3).getImm();
  if (shift_imm == 0)
    new_inst = INST_ADD_REG;
  else
    new_inst = INST_ADD_REG_SHFT;

  // Both Rn or Rm could be the base
  MachineInstr *DefMI;
  bool IsVariable = false;
  if (requirePrivilege(base_MO->getReg(), DefMI, IsVariable)
      || requirePrivilege(offset_MO->getReg(), DefMI, IsVariable)) {
    ++InstRegPrivCount;
    if (EnableXOMSFI)
      return false;

    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::tCPSI))
      .addImm(1).addImm(1);
    unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY), tmp_reg)
      .addReg(SPECIAL_REG);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::t2ADDrr), SPECIAL_REG)
      .add(MI.getOperand(1)).add(MI.getOperand(2)).add(predOps(ARMCC::AL)).add(condCodeOp());
    instChecks(MFI, MI, DefMI, new_opcode, isStore);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY), SPECIAL_REG)
      .addReg(tmp_reg);
    instPostCheck(*MI.getParent(), &MI);
    BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::tCPSI))
      .addImm(0).addImm(1);
    MI.eraseFromParent();
    return false;
  }

  ++InstRegCount;
  unsigned value_reg = value_MO->getReg();
  unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
  addOffsetInstReg(MI, MFI, new_inst, tmp_reg, base_MO, offset_MO, shift_imm);
  if (EnableXOMSFI) {
    unsigned PredReg;
    assert(getInstrPredicate(MI, PredReg)==ARMCC::AL);

    new_opcode = getSFIOpcode(new_opcode);
    if (isStore) {
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPri))
        .addReg(tmp_reg).addImm(ST_BOUNDARY).add(predOps(ARMCC::AL));

      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
        .addReg(value_reg,
                isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
        .addReg(tmp_reg, RegState::Kill).addImm(0)
        .add(predOps(ARMCC::LO)).addReg(ARM::CPSR, RegState::Implicit);
    } else {
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPri))
        .addReg(tmp_reg).addImm(LD_BOUNDARY).add(predOps(ARMCC::AL));

      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
        .addReg(value_reg,
                isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
        .addReg(tmp_reg, RegState::Kill).addImm(0)
        .add(predOps(ARMCC::HS)).addReg(ARM::CPSR, RegState::Implicit);
    }
  } else {
    new_opcode = getPrivilegedOpcode(new_opcode);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(tmp_reg, RegState::Kill).addImm(0)
      .add(predOps(ARMCC::AL));
  }

  return true;
}

bool ARMTestPass::
instImmSFI(MachineInstr &MI, MachineBasicBlock &MFI, unsigned base_reg,
           bool isStore) {
  unsigned PredReg;
  assert(getInstrPredicate(MI, PredReg)==ARMCC::AL);

  if (isStore) {
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPri))
      .addReg(base_reg).addImm(ST_BOUNDARY).add(predOps(ARMCC::AL));
    MI.getOperand(MI.findFirstPredOperandIdx()).setImm(ARMCC::LO);
  } else {
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2CMPri))
      .addReg(base_reg).addImm(LD_BOUNDARY).add(predOps(ARMCC::AL));
    MI.getOperand(MI.findFirstPredOperandIdx()).setImm(ARMCC::HS);
  }
  MI.addOperand(MachineOperand::CreateReg(ARM::CPSR, /* isDef= */ false,
                                          /* isImp= */ true));
  return false; // don't erase the MI
}

bool ARMTestPass::
instImm(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
        MachineBasicBlock &MFI, bool isStore) {
  if (!MI.getOperand(1).isReg()) {
    assert(MI.getOperand(1).isFI());
    return false;
  }

  MachineOperand *value_MO;
  unsigned idx_reg = -1; // pre/post index
  MachineOperand *base_MO;

  unsigned new_inst = INST_NONE;
  int new_inst_imm = 0;
  int offset_imm = 0;
  enum { PRE_IDX, POST_IDX, NO_IDX };
  unsigned idx_mode = NO_IDX;

  if(isT3Encoding(Opcode)){
    //Encoding T3 : LDR<c>.W <Rt>,[<Rn>{,#<imm12>}]
    value_MO = &MI.getOperand(0);
    base_MO = &MI.getOperand(1);
    new_inst_imm = MI.getOperand(2).getImm();
    if (new_inst_imm < 256) {
      offset_imm = new_inst_imm;
      dbgs() << "T3 encoding " << (isStore? "str" : "ldr") << " (imm) : offset < 256\n";
    } else {
      new_inst = INST_ADD_IMM;
      dbgs() << "T3 encoding " << (isStore? "str" : "ldr") << " (imm) : offset >= 256\n";
    }
  }else {
    assert(isT4Encoding(Opcode));
    //Encoding T4 : LDR<c> <Rt>,[<Rn>,#-<imm8>]
    int orig_imm;
    if(isT4PreEncoding(Opcode)){
      dbgs() << "T4 pre-index encoding " << (isStore? "str" : "ldr") << " (imm)\n";
      if (isStore) {
        value_MO = &MI.getOperand(1);
        idx_reg = MI.getOperand(0).getReg();
      } else {
        value_MO = &MI.getOperand(0);
        idx_reg = MI.getOperand(1).getReg();
      }
      base_MO = &MI.getOperand(2);
      orig_imm = MI.getOperand(3).getImm();
      idx_mode = PRE_IDX;

      if (orig_imm < 0) {
        new_inst = INST_SUB_IMM;
        new_inst_imm = -orig_imm;
      } else {
        new_inst = INST_ADD_IMM;
        new_inst_imm = orig_imm;
      }
    }else if(isT4PostEncoding(Opcode)){
      dbgs() << "T4 post-index encoding " << (isStore? "str" : "ldr") << " (imm)\n";
      if (isStore) {
        value_MO = &MI.getOperand(1);
        idx_reg = MI.getOperand(0).getReg();
      } else {
        value_MO = &MI.getOperand(0);
        idx_reg = MI.getOperand(1).getReg();
      }
      base_MO = &MI.getOperand(2);
      orig_imm = MI.getOperand(3).getImm();
      idx_mode = POST_IDX;

      if (orig_imm < 0) {
        new_inst = INST_SUB_IMM;
        new_inst_imm = -orig_imm;
      } else {
        new_inst = INST_ADD_IMM;
        new_inst_imm = orig_imm;
      }
    }else{
      value_MO = &MI.getOperand(0);
      base_MO = &MI.getOperand(1);
      orig_imm = MI.getOperand(2).getImm();

      assert(orig_imm < 0 && "T4 normal encoding must have negative offset");
      dbgs() << "T4 encoding " << (isStore? "str" : "ldr") << " (imm) : imm < 0\n";
      new_inst = INST_SUB_IMM;
      new_inst_imm = -orig_imm;
    }
  }

  MachineInstr *DefMI;
  unsigned base_reg = base_MO->getReg();
  // load/store that is already converted to use SP
  if (!EnableXOMSFI && base_reg==SPECIAL_REG)
    return false;

  bool IsVariable = false;
  if (requirePrivilege(base_reg, DefMI, IsVariable)) {
    if (EnableXOMSFI)
      return false;

    if (IsVariable) {
      ++InstImmPrivVarCount;

      MachineInstr *Addr = &*MRI->def_instr_begin(base_reg);
      BuildMI(*Addr->getParent(), Addr, Addr->getDebugLoc(), TII->get(ARM::tCPSI))
        .addImm(1).addImm(1);
      unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
      BuildMI(*Addr->getParent(), Addr, Addr->getDebugLoc(), TII->get(TargetOpcode::COPY), tmp_reg)
        .addReg(SPECIAL_REG);
      Addr->getOperand(0).setReg(SPECIAL_REG);
      instChecks(MFI, MI, DefMI, new_opcode, isStore);
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY), SPECIAL_REG)
        .addReg(tmp_reg);
      instPostCheck(*MI.getParent(), &MI);
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::tCPSI))
        .addImm(0).addImm(1);
      MI.eraseFromParent();
      return false;
    }

    unsigned OrigVReg = DefMI->getOperand(0).getReg();
    unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    if (DefMI->getParent() != &MFI) {
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::tCPSI))
        .addImm(1).addImm(1);
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(TargetOpcode::COPY), tmp_reg)
        .addReg(SPECIAL_REG);
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(ARM::t2MOVi32imm), SPECIAL_REG)
        .addImm(DefMI->getOperand(1).getImm());
    } else {
      BuildMI(*DefMI->getParent(), DefMI, DefMI->getDebugLoc(), TII->get(ARM::tCPSI))
        .addImm(1).addImm(1);
      BuildMI(*DefMI->getParent(), DefMI, DefMI->getDebugLoc(), TII->get(TargetOpcode::COPY), tmp_reg)
        .addReg(SPECIAL_REG);
      DefMI->getOperand(0).setReg(SPECIAL_REG);
    }
    SmallVector<MachineInstr *, 4> UseMIs;
    MachineInstr *LastUseMI;
    MachineInstr *UseMIInOtherBB = NULL;
    MachineInstr *CopyMI = NULL;
    for (MachineRegisterInfo::use_instr_iterator
           UI = MRI->use_instr_begin(base_reg), UE = MRI->use_instr_end();
         UI != UE; ++UI) {
      MachineInstr *UseMI = &*UI;
      if (UseMI->getOpcode() == TargetOpcode::COPY) {
        assert(!CopyMI);
        CopyMI = UseMI;
        continue;
      }
      if (UseMI->getParent() != &MFI) {
        // Only keep the first one encountered
        if (!UseMIInOtherBB)
          UseMIInOtherBB = UseMI;
        continue;
      }
      assert(isT3Encoding(UseMI->getOpcode()) || isT4Encoding(UseMI->getOpcode()));
      UseMIs.push_back(UseMI);
      LastUseMI = UseMI;
    }

    for (MachineInstr *UseMI : UseMIs) {
      for (unsigned i = 0, e = UseMI->getNumOperands(); i != e; ++i) {
        MachineOperand &MO = UseMI->getOperand(i);
        if (MO.isReg() && MO.getReg()==base_reg)
          MO.setReg(SPECIAL_REG);
      }
    }
    MachineBasicBlock::iterator Last(LastUseMI);
    Last++;
    unsigned Addr = DefMI->getOperand(1).getImm();
    if ((Addr >= MPU_START && Addr < MPU_END) || Addr == VTOR_ADDR)
      BuildMI(*LastUseMI->getParent(), Last, LastUseMI->getDebugLoc(), TII->get(ARM::t2MOVi), SPECIAL_REG)
        .addImm(0).add(predOps(ARMCC::AL)).add(condCodeOp());
    BuildMI(*LastUseMI->getParent(), Last, LastUseMI->getDebugLoc(), TII->get(TargetOpcode::COPY), SPECIAL_REG)
      .addReg(tmp_reg);
    instPostCheck(*LastUseMI->getParent(), Last);
    BuildMI(*LastUseMI->getParent(), Last, LastUseMI->getDebugLoc(), TII->get(ARM::tCPSI))
      .addImm(0).addImm(1);

    if (CopyMI) {
      CopyMI->setDesc(TII->get(ARM::t2MOVi32imm));
      CopyMI->getOperand(1).ChangeToImmediate(DefMI->getOperand(1).getImm());
    }

    if (UseMIInOtherBB) {
      BuildMI(*UseMIInOtherBB->getParent(), UseMIInOtherBB, UseMIInOtherBB->getDebugLoc(),
              TII->get(ARM::t2MOVi32imm), OrigVReg).addImm(DefMI->getOperand(1).getImm());
    }

    return false;
  }

  if (EnableXOMSFI)
    return instImmSFI(MI, MFI, base_MO->getReg(), isStore);

  unsigned value_reg = value_MO->getReg();
  new_opcode = getPrivilegedOpcode(new_opcode);
  switch (idx_mode) {
  case PRE_IDX: {
    ++InstImmPreIdxCount;
    addOffsetInstImm(MI, MFI, new_inst, idx_reg, base_MO, new_inst_imm);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(idx_reg).addImm(offset_imm)
      .add(predOps(ARMCC::AL));
    break;
  }
  case POST_IDX: {
    ++InstImmPostIdxCount;
    addOffsetInstImm(MI, MFI, new_inst, idx_reg, base_MO, new_inst_imm);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(base_MO->getReg(), base_MO->isKill() ? RegState::Kill : 0).addImm(offset_imm)
      .add(predOps(ARMCC::AL));
    break;
  }
  case NO_IDX: {
    if (new_inst != INST_NONE) {
      ++InstImmNoIdxOffsetCount;
      unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
      addOffsetInstImm(MI, MFI, new_inst, tmp_reg, base_MO, new_inst_imm);
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
        .addReg(value_reg,
                isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
        .addReg(tmp_reg, RegState::Kill).addImm(offset_imm)
        .add(predOps(ARMCC::AL));
    } else {
      ++InstImmNoIdxCount;
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
        .addReg(value_reg,
                isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
        .addReg(base_MO->getReg(), base_MO->isKill() ? RegState::Kill : 0).addImm(offset_imm)
        .add(predOps(ARMCC::AL));
    }
    break;
  }
  }

  return true;
}

bool ARMTestPass::
instLDSTDouble(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
               MachineBasicBlock &MFI, bool isStore) {
  MachineOperand *value1_MO = &MI.getOperand(0);
  unsigned value1_reg = value1_MO->getReg();
  MachineOperand *value2_MO = &MI.getOperand(1);
  unsigned value2_reg = value2_MO->getReg();
  MachineOperand *base_MO = &MI.getOperand(2);
  int orig_imm = MI.getOperand(3).getImm();
  unsigned new_inst = INST_NONE;
  int new_inst_imm = 0;
  int offset_imm = 0;

  MachineInstr *DefMI;
  bool IsVariable = false;
  if (requirePrivilege(base_MO->getReg(), DefMI, IsVariable)) {
    assert(0);
    return false;
  }

  if (orig_imm < 0) {
    dbgs() << (isStore? "strd" : "ldrd") << " (imm) : imm < 0\n";
    new_inst = INST_SUB_IMM;
    new_inst_imm = (-orig_imm);
  } else {
    new_inst_imm = orig_imm;
    if ((new_inst_imm+4) < 256) {
      dbgs() << (isStore? "strd" : "ldrd") << " (imm) : imm+4 < 256\n";
      offset_imm = new_inst_imm;
    } else {
      dbgs() << (isStore? "strd" : "ldrd") << " (imm) : imm+4 >= 256\n";
      new_inst = INST_ADD_IMM;
    }
  }

  if (EnableXOMSFI)
    return instImmSFI(MI, MFI, base_MO->getReg(), isStore);

  new_opcode = getPrivilegedOpcode(new_opcode);
  if (new_inst != INST_NONE) {
    ++InstDoubleOffsetCount;
    unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    addOffsetInstImm(MI, MFI, new_inst, tmp_reg, base_MO, new_inst_imm);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value1_reg,
              isStore? (value1_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(tmp_reg).addImm(offset_imm)
      .add(predOps(ARMCC::AL));
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value2_reg,
              isStore? (value2_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(tmp_reg, RegState::Kill).addImm(offset_imm+4)
      .add(predOps(ARMCC::AL));
  } else {
    ++InstDoubleCount;
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value1_reg,
              isStore? (value1_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(base_MO->getReg(), base_MO->isKill() ? RegState::Kill : 0).addImm(offset_imm)
      .add(predOps(ARMCC::AL));
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value2_reg,
              isStore? (value2_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(base_MO->getReg(), base_MO->isKill() ? RegState::Kill : 0).addImm(offset_imm+4)
      .add(predOps(ARMCC::AL));
  }

  return true;
}

bool ARMTestPass::instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
    //LDR (immediate)
  case ARM::t2LDRi12:
  case ARM::t2LDRi8:
  case ARM::t2LDR_PRE:
  case ARM::t2LDR_POST:
    return instImm(Opcode, XOM_LD_W, MI, MFI, false);

    //LDR (literal) skip
    //LDR (register)
  case ARM::t2LDRs:
    return instReg(Opcode, XOM_LD_W, MI, MFI, false);

    //LDRH (immediate)
  case ARM::t2LDRHi12:
  case ARM::t2LDRHi8:
  case ARM::t2LDRH_PRE:
  case ARM::t2LDRH_POST:
    return instImm(Opcode, XOM_LD_H, MI, MFI, false);

    //LDRH (literal) skip
    //LDRH (register)
  case ARM::t2LDRHs:
    return instReg(Opcode, XOM_LD_H, MI, MFI, false);

    //LDRB (immediate)
  case ARM::t2LDRBi12:
  case ARM::t2LDRBi8:
  case ARM::t2LDRB_PRE:
  case ARM::t2LDRB_POST:
    return instImm(Opcode, XOM_LD_B, MI, MFI, false);

    //LDRB (literal) skip
    //LDRB (register)
  case ARM::t2LDRBs:
    return instReg(Opcode, XOM_LD_B, MI, MFI, false);

    //LDRSH (immediate)
  case ARM::t2LDRSHi12:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSH_PRE:
  case ARM::t2LDRSH_POST:
    return instImm(Opcode, XOM_LD_SH, MI, MFI, false);

    //LDRSH (literal) skip
    //LDRSH (register)
  case ARM::t2LDRSHs:
    return instReg(Opcode, XOM_LD_SH, MI, MFI, false);

    //LDRSB (immediate)
  case ARM::t2LDRSBi12:
  case ARM::t2LDRSBi8:
  case ARM::t2LDRSB_PRE:
  case ARM::t2LDRSB_POST:
    return instImm(Opcode, XOM_LD_SB, MI, MFI, false);

    //LDRSB (literal) skip
    //LDRSB (register)
  case ARM::t2LDRSBs:
    return instReg(Opcode, XOM_LD_SB, MI, MFI, false);

    //LDRD
  case ARM::t2LDRDi8:
    return instLDSTDouble(Opcode, XOM_LD_W, MI, MFI, false);

    // Don't need to handle load from constant pool
  case ARM::t2LDRpci:
  case ARM::t2LDRpci_pic:
    return false;

    //STR (immediate)
  case ARM::t2STRi12:
  case ARM::t2STRi8:
  case ARM::t2STR_PRE:
  case ARM::t2STR_POST:
    return instImm(Opcode, XOM_ST_W, MI, MFI, true);

    //STR (literal) skip
    //STR (register)
  case ARM::t2STRs:
    return instReg(Opcode, XOM_ST_W, MI, MFI, true);

    //STRH (immediate)
  case ARM::t2STRHi12:
  case ARM::t2STRHi8:
  case ARM::t2STRH_PRE:
  case ARM::t2STRH_POST:
    return instImm(Opcode, XOM_ST_H, MI, MFI, true);

    //STRH (litenal) skip
    //STRH (register)
  case ARM::t2STRHs:
    return instReg(Opcode, XOM_ST_H, MI, MFI, true);

    //STRB (immediate)
  case ARM::t2STRBi12:
  case ARM::t2STRBi8:
  case ARM::t2STRB_PRE:
  case ARM::t2STRB_POST:
    return instImm(Opcode, XOM_ST_B, MI, MFI, true);

    //STRB (literal) skip
    //STRB (register)
  case ARM::t2STRBs:
    return instReg(Opcode, XOM_ST_B, MI, MFI, true);

    //STRD
  case ARM::t2STRDi8:
    return instLDSTDouble(Opcode, XOM_ST_W, MI, MFI, true);

  case TargetOpcode::DBG_VALUE: case TargetOpcode::COPY: case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::INLINEASM:

  case ARM::ADJCALLSTACKUP: case ARM::ADJCALLSTACKDOWN: case ARM::PHI: case ARM::MEMCPY:
  case ARM::tPICADD:

  case ARM::t2MOVi: case ARM::t2MOVi16: case ARM::t2MOVi32imm:
  case ARM::t2MOVsrl_flag: case ARM::t2MOVsra_flag:
  case ARM::t2MOVCCi: case ARM::t2MOVCCi16: case ARM::t2MOVCCi32imm: case ARM::t2MOVCCr:
  case ARM::t2MOVCCasr: case ARM::t2MOVCClsl: case ARM::t2MOVCClsr:
  case ARM::t2MVNi: case ARM::t2MVNr: case ARM::t2MVNCCi:
  case ARM::t2LEApcrelJT:
  case ARM::t2SXTH: case ARM::t2SXTB: case ARM::t2UXTH: case ARM::t2UXTB:
  case ARM::t2ADDri: case ARM::t2ADDrr: case ARM::t2ADDrs: case ARM::t2ADDri12:
  case ARM::t2ADCri: case ARM::t2ADCrr: case ARM::t2ADCrs:
  case ARM::t2SUBri: case ARM::t2SUBrr: case ARM::t2SUBrs: case ARM::t2SUBri12:
  case ARM::t2SBCri: case ARM::t2SBCrr: case ARM::t2SBCrs:
  case ARM::t2RSBri: case ARM::t2RSBrs:
  case ARM::t2MUL: case ARM::t2MLA: case ARM::t2MLS:
  case ARM::t2SMLAL: case ARM::t2SMULL: case ARM::t2UMLAL: case ARM::t2UMULL:
  case ARM::t2SDIV: case ARM::t2UDIV:
  case ARM::t2ANDri: case ARM::t2ANDrr: case ARM::t2ANDrs:
  case ARM::t2ORRri: case ARM::t2ORRrr: case ARM::t2ORRrs:
  case ARM::t2ORNri: case ARM::t2ORNrr:
  case ARM::t2EORrr: case ARM::t2EORri: case ARM::t2EORrs:
  case ARM::t2ASRri: case ARM::t2ASRrr:
  case ARM::t2LSLri: case ARM::t2LSLrr: case ARM::t2LSRri: case ARM::t2LSRrr:
  case ARM::t2RORri: case ARM::t2RORrr: case ARM::t2RRX:
  case ARM::t2BICrr: case ARM::t2BICri: case ARM::t2BICrs: case ARM::t2BFC: case ARM::t2BFI:
  case ARM::t2SBFX: case ARM::t2UBFX:
  case ARM::t2CLZ:
  case ARM::t2RBIT: case ARM::t2REV: case ARM::t2REV16:

  case ARM::t2CMNri: case ARM::t2CMNzrr:
  case ARM::t2CMPri: case ARM::t2CMPrr: case ARM::t2CMPrs:
  case ARM::t2TSTri: case ARM::t2TSTrr: case ARM::t2TSTrs:
  case ARM::t2TEQri: case ARM::t2TEQrr: case ARM::t2TEQrs:
  case ARM::tBL: case ARM::tBLXr: case ARM::tBX_RET:
  case ARM::t2B: case ARM::t2Bcc: case ARM::t2BR_JT:
  case ARM::TCRETURNdi: case ARM::TCRETURNri:

  case ARM::tCPSI: // inserted by this pass
    return false;

  default: {
    dbgs() << "warning: unhandled opcode " <<  TII->getName(MI.getOpcode()) << '\n';
    assert(0);
    return false;
  }
  }
  return false;
}


bool ARMTestPass::runOnMachineFunction(MachineFunction &Fn) {
  // TD = &Fn.getDataLayout();
  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  // TRI = STI->getRegisterInfo();
  MRI = &Fn.getRegInfo();
  // MF  = &Fn;

  bool Modified = false;
  if(Fn.getName().compare(StringRef("__sfputc_r")) == 0)
    return Modified;

  for (MachineBasicBlock &MFI : Fn) {
    /*
    if(Fn.getName().compare(StringRef("SolveCubic")) != 0){
          // dbgs() << "Current function name: [" << Fn.getName() << "]\n";
          continue;
        }
    */
    //    dbgs() << "Current function name: [" << Fn.getName() << "]\n";
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      if (instMemoryOp(MI, MFI)) {
        MI.eraseFromParent();
      }
      MBBI = NMBBI;
    }
  }

  dbgs() << "Current function name   : " << Fn.getName() << "\n";
  dbgs() << "InstRegCount            : " << InstRegCount  << "\n";
  dbgs() << "InstImmPreIdxCount      : " << InstImmPreIdxCount  << "\n";
  dbgs() << "InstImmPostIdxCount     : " << InstImmPostIdxCount  << "\n";
  dbgs() << "InstImmNoIdxOffsetCount : " << InstImmNoIdxOffsetCount  << "\n";
  dbgs() << "InstImmNoIdxCount       : " << InstImmNoIdxCount  << "\n";
  dbgs() << "InstDoubleOffsetCount   : " << InstDoubleOffsetCount  << "\n";
  dbgs() << "InstDoubleCount         : " << InstDoubleCount  << "\n";
  dbgs() << "InstRegPrivCount        : " << InstRegPrivCount  << "\n";
  dbgs() << "InstImmPrivVarCount     : " << InstImmPrivVarCount  << "\n";
  dbgs() << "InstImmPrivCount        : " << InstImmPrivCount  << "\n";

  return Modified;
}

FunctionPass *llvm::createARMTestPassPass() {
  return new ARMTestPass();
}
