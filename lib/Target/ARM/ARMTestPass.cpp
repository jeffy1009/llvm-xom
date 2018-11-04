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

	int count = 0;

    bool requirePrivilege(unsigned Reg);

    void addOffsetInstReg(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                          unsigned dest_reg, MachineOperand *reg1, MachineOperand *reg2,
                          int shift_imm);
    void addOffsetInstImm(MachineInstr &MI, MachineBasicBlock &MFI, unsigned insttype,
                          unsigned dest_reg, MachineOperand *reg1, int imm);

    bool instReg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                 MachineBasicBlock &MFI, bool isStore);
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

static bool isRegT1Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::tLDRr: case ARM::tLDRHr: case ARM::tLDRBr:
  case ARM::tSTRr: case ARM::tSTRHr: case ARM::tSTRBr:
    return true;
  default:
    return false;
  }
}

static bool isRegT2Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDRs: case ARM::t2LDRHs: case ARM::t2LDRBs:
  case ARM::t2LDRSHs: case ARM::t2LDRSBs:
  case ARM::t2STRs: case ARM::t2STRHs: case ARM::t2STRBs:
    return true;
  default:
    return false;
  }
}

static bool isT1Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::tLDRi: case ARM::tLDRHi: case ARM::tLDRBi:
  case ARM::tSTRi: case ARM::tSTRHi: case ARM::tSTRBi:
    return true;
  default:
    return false;
  }
}
/*
static bool isT2Encoding(unsigned Opcode){
  //currently, we do not consider T2 encoding
  return false;
}
*/
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

bool ARMTestPass::requirePrivilege(unsigned Reg) {
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
    DEBUG(MI->dump());
    return true;
  }
  case TargetOpcode::COPY:
    if (MI->getOperand(1).getReg()==ARM::SP)
      return false;
    // fall-through
  case ARM::t2ADDri: case ARM::t2ADDrr: case ARM::t2ADDri12:
    return MI->getOperand(1).isReg() && requirePrivilege(MI->getOperand(1).getReg());
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

bool ARMTestPass::
instReg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
        MachineBasicBlock &MFI, bool isStore) {
  MachineOperand *value_MO = &MI.getOperand(0);
  MachineOperand *base_MO = &MI.getOperand(1);
  MachineOperand *offset_MO = &MI.getOperand(2);

  unsigned new_inst = INST_NONE;
  int shift_imm = 0;

  if(isRegT1Encoding(Opcode)){
    assert(0 && "error: must verify T1 encoding handling");
    // LDR<c> <Rt>,[<Rn>,<Rm>]
    dbgs() << "T1 encoding for " << (isStore? "str" : "ldr") << " (reg)\n";
    new_inst = INST_ADD_REG;
  }else if(isRegT2Encoding(Opcode)){
    // LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}]
    dbgs() << "T2 encoding for " << (isStore? "str" : "ldr") << " (reg)\n";
    shift_imm = MI.getOperand(3).getImm();
    if (shift_imm == 0)
      new_inst = INST_ADD_REG;
    else
      new_inst = INST_ADD_REG_SHFT;
  }else{
    return false;
  }

  // Both Rn or Rm could be the base
  if (requirePrivilege(base_MO->getReg()) || requirePrivilege(offset_MO->getReg()))
    return false;

  unsigned value_reg = value_MO->getReg();
  if(new_inst != INST_NONE) {
    unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    addOffsetInstReg(MI, MFI, new_inst, tmp_reg, base_MO, offset_MO, shift_imm);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(tmp_reg, RegState::Kill).addImm(0)
      .add(predOps(ARMCC::AL));
  } else {
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(base_MO->getReg(), base_MO->isKill() ? RegState::Kill : 0).addImm(0)
      .add(predOps(ARMCC::AL));
  }

  return true;
}

bool ARMTestPass::
instImm(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
        MachineBasicBlock &MFI, bool isStore) {
  MachineOperand *value_MO;
  unsigned idx_reg = -1; // pre/post index
  MachineOperand *base_MO;

  unsigned new_inst = INST_NONE;
  int new_inst_imm = 0;
  int offset_imm = 0;
  enum { PRE_IDX, POST_IDX, NO_IDX };
  unsigned idx_mode = NO_IDX;

  if(isT1Encoding(Opcode)){
    assert(0 && "error: must verify T1 encoding handling");
    //Encoding T1 : LDR<c> <Rt>, [<Rn>{,#<imm5>}]
    dbgs() << "T1 encoding " << (isStore? "str" : "ldr") << " (imm)\n";
    value_MO = &MI.getOperand(0);
    base_MO = &MI.getOperand(1);
    offset_imm = MI.getOperand(2).getImm();
  }else if(isT3Encoding(Opcode)){
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
  }else if(isT4Encoding(Opcode)){
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

      if (orig_imm < 0) {
        dbgs() << "T4 encoding " << (isStore? "str" : "ldr") << " (imm) : imm < 0\n";
        new_inst = INST_SUB_IMM;
        new_inst_imm = -orig_imm;
      } else {
        dbgs() << "T4 encoding " << (isStore? "str" : "ldr") << " (imm) : imm >= 0\n";
        assert(orig_imm < 256);
        offset_imm = orig_imm;
      }
    }
  }else{
    return false;
  }

  if (requirePrivilege(base_MO->getReg()))
    return false;

  unsigned value_reg = value_MO->getReg();
  switch (idx_mode) {
  case PRE_IDX: {
    addOffsetInstImm(MI, MFI, new_inst, idx_reg, base_MO, new_inst_imm);
    BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
      .addReg(value_reg,
              isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
      .addReg(idx_reg).addImm(offset_imm)
      .add(predOps(ARMCC::AL));
    break;
  }
  case POST_IDX: {
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
      unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
      addOffsetInstImm(MI, MFI, new_inst, tmp_reg, base_MO, new_inst_imm);
      BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
        .addReg(value_reg,
                isStore? (value_MO->isKill() ? RegState::Kill : 0) : RegState::Define)
        .addReg(tmp_reg, RegState::Kill).addImm(offset_imm)
        .add(predOps(ARMCC::AL));
    } else {
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

  if (requirePrivilege(base_MO->getReg()))
    return false;

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

  if (new_inst != INST_NONE) {
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
  case ARM::t2LDRi12:       //checked
  case ARM::t2LDRi8:        //not checked
  case ARM::t2LDR_PRE:
  case ARM::t2LDR_POST:
    return instImm(Opcode, ARM::t2LDRT, MI, MFI, false);

    //LDR (literal) skip
    //LDR (register)
  case ARM::t2LDRs:         //instrumented but not checked
    return instReg(Opcode, ARM::t2LDRT, MI, MFI, false);

    //LDRH (immediate)
  case ARM::t2LDRHi12:
  case ARM::t2LDRHi8:
  case ARM::t2LDRH_PRE:
  case ARM::t2LDRH_POST:
    return instImm(Opcode, ARM::t2LDRHT, MI, MFI, false);

    //LDRH (litenal) skip
    //LDRH (register)
  case ARM::t2LDRHs:
    return instReg(Opcode, ARM::t2LDRHT, MI, MFI, false);

    //LDRB (immediate)
  case ARM::t2LDRBi12:
  case ARM::t2LDRBi8:
  case ARM::t2LDRB_PRE:
  case ARM::t2LDRB_POST:
    return instImm(Opcode, ARM::t2LDRBT, MI, MFI, false);

    //LDRB (literal) skip
    //LDRB (register)
  case ARM::t2LDRBs:
    return instReg(Opcode, ARM::t2LDRBT, MI, MFI, false);

    //LDRSH (immediate)
  case ARM::t2LDRSHi12:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSH_PRE:
  case ARM::t2LDRSH_POST:
    return instImm(Opcode, ARM::t2LDRSHT, MI, MFI, false);

    //LDRSH (litenal) skip
    //LDRSH (register)
  case ARM::t2LDRSHs:
    return instReg(Opcode, ARM::t2LDRSHT, MI, MFI, false);

    //LDRSB (immediate)
  case ARM::t2LDRSBi12:
  case ARM::t2LDRSBi8:
  case ARM::t2LDRSB_PRE:
  case ARM::t2LDRSB_POST:
    return instImm(Opcode, ARM::t2LDRSBT, MI, MFI, false);

    //LDRSB (literal) skip
    //LDRSB (register)
  case ARM::t2LDRSBs:
    return instReg(Opcode, ARM::t2LDRSBT, MI, MFI, false);

    //[TODO] load double, load multiple, float load
    //LDRD
  case ARM::t2LDRDi8:
    return instLDSTDouble(Opcode, ARM::t2LDRT, MI, MFI, false);

    // Don't need to handle load from constant pool
  case ARM::t2LDRpci:
  case ARM::t2LDRpci_pic:
    return false;

    //VLDR
  case ARM::VLDRS:
  case ARM::VLDRD:
    break;

    //STR (immediate)
  case ARM::t2STRi12:       //checked
  case ARM::t2STRi8:        //not checked
  case ARM::t2STR_PRE:
  case ARM::t2STR_POST:
    return instImm(Opcode, ARM::t2STRT, MI, MFI, true);

    //STR (literal) skip
    //STR (register)
  case ARM::t2STRs:         //instrumented but not checked
    return instReg(Opcode, ARM::t2STRT, MI, MFI, true);

    //STRH (immediate)
  case ARM::t2STRHi12:
  case ARM::t2STRHi8:
  case ARM::t2STRH_PRE:
  case ARM::t2STRH_POST:
    return instImm(Opcode, ARM::t2STRHT, MI, MFI, true);

    //STRH (litenal) skip
    //STRH (register)
  case ARM::t2STRHs:
    return instReg(Opcode, ARM::t2STRHT, MI, MFI, true);

    //STRB (immediate)
  case ARM::t2STRBi12:
  case ARM::t2STRBi8:
  case ARM::t2STRB_PRE:
  case ARM::t2STRB_POST:
    return instImm(Opcode, ARM::t2STRBT, MI, MFI, true);

    //STRB (literal) skip
    //STRB (register)
  case ARM::t2STRBs:
    return instReg(Opcode, ARM::t2STRBT, MI, MFI, true);

    //[TODO] load double, load multiple, float load
    //STRD
  case ARM::t2STRDi8:
    return instLDSTDouble(Opcode, ARM::t2STRT, MI, MFI, true);

    //VSTR
  case ARM::VSTRS:
  case ARM::VSTRD:
    break;
  default:
    return false;
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
        count++;
      }
      MBBI = NMBBI;
      //        dbgs() << "instrumented !!!!! : " << count  << "\n";
    }
  }

  return Modified;
}

FunctionPass *llvm::createARMTestPassPass() {
  return new ARMTestPass();
}
