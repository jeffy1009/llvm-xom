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

#define INST_NONE 0
#define INST_ADD 1
#define INST_ADD_REG_IMM 6
#define INST_ADD_REG 7
#define INST_LSL 2
#define INST_SUB 3
#define INST_SUB_REG_IMM 5
#define INST_SUB_REG 8
#define INST_LSR 4

static bool isLdrRegT1Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::tLDRr:
  case ARM::tLDRHr:
  case ARM::tLDRBr:
    return true;
  default:
    return false;
  }
  return false;
}

static bool isLdrRegT2Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDRs:
  case ARM::t2LDRHs:
  case ARM::t2LDRBs:
  case ARM::t2LDRSHs:
  case ARM::t2LDRSBs:
    return true;
  default:
    return false;
  }
  return false;
}

static bool isT1Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::tLDRi:
  case ARM::tLDRHi:
  case ARM::tLDRBi:
    return true;
  default:
    return false;
  }
  return false;
}
/*
static bool isT2Encoding(unsigned Opcode){
  //currently, we do not consider T2 encoding
  return false;
}
*/
static bool isT3Encoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDRi12:
  case ARM::t2LDRHi12:
  case ARM::t2LDRBi12:
  case ARM::t2LDRSHi12:
  case ARM::t2LDRSBi12:
    return true;
  default:
    return false;
  }
  return false;
}

static bool isT4Encoding(unsigned Opcode){
switch(Opcode){
  case ARM::t2LDRi8:
  case ARM::t2LDRHi8:
  case ARM::t2LDRBi8:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSBi8:

 case ARM::t2LDR_PRE:
 case ARM::t2LDR_POST:
    // case ARM::LDR_PRE_IMM:
    // case ARM::LDR_POST_IMM:

 case ARM::t2LDRH_PRE:
 case ARM::t2LDRH_POST:
    // case ARM::LDRH_PRE:
    // case ARM::LDRH_POST:

 case ARM::t2LDRB_PRE:
 case ARM::t2LDRB_POST:
    // case ARM::LDRB_PRE_IMM:
    // case ARM::LDRB_POST_IMM:

 case ARM::t2LDRSH_PRE:
 case ARM::t2LDRSH_POST:
    // case ARM::LDRSH_PRE:
    // case ARM::LDRSH_POST:

 case ARM::t2LDRSB_PRE:
 case ARM::t2LDRSB_POST:
    // case ARM::LDRSB_PRE:
    // case ARM::LDRSB_POST:
    return true;
  default:
    return false;
  }
  return false;
}

static bool isT4PreEncoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDR_PRE:
    //  case ARM::LDR_PRE_IMM:

  case ARM::t2LDRH_PRE:
    // case ARM::LDRH_PRE:

 case ARM::t2LDRB_PRE:
    // case ARM::LDRB_PRE_IMM:

  case ARM::t2LDRSH_PRE:
    // case ARM::LDRSH_PRE:

 case ARM::t2LDRSB_PRE:
    // case ARM::LDRSB_PRE:
    return true;
  default:
    return false;
  }
  return false;
}

static bool isT4PostEncoding(unsigned Opcode){
  switch(Opcode){
  case ARM::t2LDR_POST:
    //  case ARM::LDR_POST_IMM:

 case ARM::t2LDRH_POST:
   // case ARM::LDRH_POST:

 case ARM::t2LDRB_POST:
   // case ARM::LDRB_POST_IMM:

 case ARM::t2LDRSH_POST:
   // case ARM::LDRSH_POST:

 case ARM::t2LDRSB_POST:
   // case ARM::LDRSB_POST:
    return true;
  default:
    return false;
  }
  return false;
}


static unsigned transT1Imm(unsigned Opcode, int imm){
  switch(Opcode){
  case ARM::tLDRi:
    imm = imm << 2;
    break;
  case ARM::tLDRHi:
    imm = imm << 1;
    break;
  case ARM::tLDRBi:
    break;
  }
  return imm;
}

static void transT4Inst(const unsigned Opcode, unsigned &pre_inst,
                            unsigned &post_inst, int &imm_inst, int &offset_imm){

    if(isT4PreEncoding(Opcode)){
      if(offset_imm < 0){
        pre_inst = INST_SUB;
        post_inst = INST_NONE;
        imm_inst = -offset_imm;
        offset_imm = 0;
      }else{
        pre_inst = INST_ADD;
        post_inst = INST_NONE;
        imm_inst = offset_imm;
        offset_imm = 0;
      }
    }else if(isT4PostEncoding(Opcode)){
      if(offset_imm < 0){
        pre_inst = INST_NONE;
        post_inst = INST_SUB;
        imm_inst = -offset_imm;
        offset_imm = 0;
      }else{
        pre_inst = INST_NONE;
        post_inst = INST_ADD;
        imm_inst = offset_imm;
        offset_imm = 0;
      }
    }else{
      if(offset_imm < 0){
        pre_inst = INST_SUB;
        post_inst = INST_ADD;
        imm_inst = -offset_imm;
        offset_imm = 0;
      }
    }
}

static void addOffsetInst(MachineInstr &MI, MachineBasicBlock &MFI,
                          const TargetInstrInfo* TII, unsigned insttype,
                          int imm, int imm2, unsigned reg1, unsigned reg2) {
  if(insttype == INST_ADD){

    AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDri12))
    .addReg(reg1, RegState::Define)
    .addReg(reg1, RegState::Kill)
    .addImm(imm2));

  }else if(insttype == INST_SUB){

    AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2SUBri12))
    .addReg(reg1, RegState::Define)
    .addReg(reg1, RegState::Kill)
    .addImm(imm2));

  }else if(insttype == INST_ADD_REG){

     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tADDrr))
                    .addReg(reg1, RegState::Define)
                    .addReg(ARM::CPSR, RegState::Define)
                    .addReg(reg1).addReg(reg2));

  }else if(insttype == INST_SUB_REG){
     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tSUBrr))
                    .addReg(reg1, RegState::Define)
                    .addReg(ARM::CPSR, RegState::Define)
                    .addReg(reg1).addReg(reg2));

  }else if(insttype == INST_ADD_REG_IMM){
     // mov.w rm, rm, lsl #imm
     // adds rn, rn, rm
     AddDefaultCC(AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2LSLri))
                                 .addReg(reg2, RegState::Define)
                                 .addReg(reg2).addImm(imm2)));

     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tADDrr))
                    .addReg(reg1, RegState::Define)
                    .addReg(ARM::CPSR, RegState::Define)
                    .addReg(reg1).addReg(reg2));

  }else if(insttype == INST_SUB_REG_IMM){
    // subs rn, rn, rm
    // mov.w rm, rm, lsl #imm
     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tSUBrr))
                    .addReg(reg1, RegState::Define)
                    .addReg(ARM::CPSR, RegState::Define)
                    .addReg(reg1).addReg(reg2));

     AddDefaultCC(AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2LSRri))
                                 .addReg(reg2, RegState::Define)
                                 .addReg(reg2).addImm(imm2)));
  }

  return;
}

static bool instLDRreg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                       MachineBasicBlock &MFI, const TargetInstrInfo* TII){
  unsigned dest_reg = MI.getOperand(0).getReg();
  unsigned base_reg = MI.getOperand(1).getReg();
  unsigned offset_reg = MI.getOperand(2).getReg();
  int offset_imm;
  //unsigned new_opcode = ARM::t2LDRi12;
  //unsigned new_opcode = ARM::t2LDRT;
  unsigned pre_inst = INST_NONE;
  unsigned post_inst = INST_NONE;
  int imm_inst = INST_NONE;


  if(isLdrRegT1Encoding(Opcode)){
    // LDR<c> <Rt>,[<Rn>,<Rm>]
    dbgs() << "T1 encoding for ldr (reg) case\n";

    pre_inst = INST_ADD_REG;
    post_inst = INST_SUB_REG;
    offset_imm = 0;

  }else if(isLdrRegT2Encoding(Opcode)){
    dbgs() << "T2 encoding for ldr (reg) case\n";
    //  LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}]
    offset_imm = MI.getOperand(3).getImm();
    //    assert(offset_imm == 0 && "dhkwon test");

    pre_inst = INST_ADD_REG_IMM;
    post_inst = INST_SUB_REG_IMM;
    imm_inst = offset_imm;
    offset_imm = 0;
  }else{
    return false;
  }

  if(pre_inst != INST_NONE) addOffsetInst(MI, MFI, TII, pre_inst,
                                          offset_imm, imm_inst, base_reg, offset_reg);

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
                 .addReg(dest_reg, RegState::Define)
                 .addReg(base_reg)
                 .addImm(offset_imm));

  if(post_inst != INST_NONE) addOffsetInst(MI, MFI, TII, post_inst,
                                           offset_imm, imm_inst, base_reg, offset_reg);

  return true;
}



static bool instLDRimm(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                       MachineBasicBlock &MFI, const TargetInstrInfo* TII){
  return false;
  unsigned dest_reg = MI.getOperand(0).getReg();
  unsigned base_reg = MI.getOperand(1).getReg();
  unsigned offset_reg = 0;
  int offset_imm;
  //unsigned new_opcode = ARM::t2LDRi12;
  //unsigned new_opcode = ARM::t2LDRT;
  unsigned pre_inst = INST_NONE;
  unsigned post_inst = INST_NONE;
  int imm_inst = INST_NONE;

  if(isT1Encoding(Opcode)){
    //Encoding T1 : LDR<c> <Rt>, [<Rn>{,#<imm5>}]
    dbgs() << "T1 encoding ldr (imm) case\n";

    offset_imm = MI.getOperand(2).getImm();

    offset_imm = transT1Imm(Opcode, offset_imm);

  }else if(isT3Encoding(Opcode)){
    //Encoding T3 : LDR<c>.W <Rt>,[<Rn>{,#<imm12>}]
    dbgs() << "T3 encoding ldr (imm) case\n";

    offset_imm = MI.getOperand(2).getImm();

    pre_inst = INST_ADD;
    post_inst = INST_SUB;
    imm_inst = offset_imm;
    offset_imm = 0;

  }else if(isT4Encoding(Opcode)){
    //Encoding T4 : LDR<c> <Rt>,[<Rn>,#-<imm8>]

    if(isT4PreEncoding(Opcode)){
      dbgs() << "T4 pre-index encoding ldr (imm) case\n";
      offset_imm = MI.getOperand(3).getImm();

      transT4Inst(Opcode, pre_inst, post_inst,
                  imm_inst, offset_imm);
    }else if(isT4PostEncoding(Opcode)){
      dbgs() << "T4 post-index encoding ldr (imm) case\n";
      offset_imm = MI.getOperand(3).getImm();

      transT4Inst(Opcode, pre_inst, post_inst,
                  imm_inst, offset_imm);
    }else{
      dbgs() << "T4 encoding ldr (imm) case\n";
      offset_imm = MI.getOperand(2).getImm();

      transT4Inst(Opcode, pre_inst, post_inst,
                 imm_inst, offset_imm);
    }
  }else{
    return false;
  }

  if(pre_inst != INST_NONE) addOffsetInst(MI, MFI, TII, pre_inst,
                                          offset_imm, imm_inst, base_reg, offset_reg);

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
               .addReg(dest_reg, RegState::Define)
               .addReg(base_reg)
               .addImm(offset_imm));

  if(post_inst != INST_NONE) addOffsetInst(MI, MFI, TII, post_inst,
                                           offset_imm, imm_inst, base_reg, offset_reg);

  return true;
}

/// Returns true if instruction is a memory operation that this pass is capable
/// of operating on.
static bool instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI,
                         const TargetInstrInfo* TII) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {

    //LDR (immediate)
  case ARM::tLDRi:          //checked
    //  case ARM::tLDRspi:  //stack base load skip
  case ARM::t2LDRi12:       //checked
  case ARM::t2LDRi8:        //not checked
  case ARM::t2LDR_PRE:
  case ARM::t2LDR_POST:
    //  case ARM::LDR_PRE_IMM:
    //  case ARM::LDR_POST_IMM:
    return instLDRimm(Opcode, ARM::t2LDRT, MI, MFI, TII);
    //LDR (literal) skip
    //LDR (register)
    //  case ARM::LDRrs:
  case ARM::tLDRr:
  case ARM::t2LDRs:         //instrumented but not checked
    //    return false;
    return instLDRreg(Opcode, ARM::t2LDRT, MI, MFI, TII);

    //LDRH (immediate)
    //  case ARM::LDRH:           //not used??
  case ARM::tLDRHi:
  case ARM::t2LDRHi12:
  case ARM::t2LDRHi8:
  case ARM::t2LDRH_PRE:
  case ARM::t2LDRH_POST:
    //  case ARM::LDRH_PRE:
    //  case ARM::LDRH_POST:
    return instLDRimm(Opcode, ARM::t2LDRHT, MI, MFI, TII);
    //LDRH (litenal) skip
    //LDRH (register)
  case ARM::t2LDRHs:
  case ARM::tLDRHr:
    return instLDRreg(Opcode, ARM::t2LDRHT, MI, MFI, TII);

    //LDRB (immediate)
    //  case ARM::LDRBi12:        //not used??
  case ARM::tLDRBi:
  case ARM::t2LDRBi12:
  case ARM::t2LDRBi8:
  case ARM::t2LDRB_PRE:
  case ARM::t2LDRB_POST:
    //  case ARM::LDRB_PRE_IMM:
    //  case ARM::LDRB_POST_IMM:
    return instLDRimm(Opcode, ARM::t2LDRBT, MI, MFI, TII);
    //LDRB (literal) skip
    //LDRB (register)
  case ARM::t2LDRBs:
  case ARM::tLDRBr:
    return instLDRreg(Opcode, ARM::t2LDRBT, MI, MFI, TII);

    //LDRSH (immediate)
    //  case ARM::LDRSH:          //not used??
  case ARM::tLDRSH:               //not used??
  case ARM::t2LDRSHi12:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSH_PRE:
  case ARM::t2LDRSH_POST:
    //  case ARM::LDRSH_PRE:
    //  case ARM::LDRSH_POST:
    return instLDRimm(Opcode, ARM::t2LDRSHT, MI, MFI, TII);
    //LDRSH (litenal) skip
    //LDRSH (register)
 case ARM::t2LDRSHs:
   return instLDRreg(Opcode, ARM::t2LDRSHT, MI, MFI, TII);

    //LDRSB (immediate)
   //  case ARM::LDRSB:           //not used??
  case ARM::tLDRSB:
  case ARM::t2LDRSBi12:
  case ARM::t2LDRSBi8:
  case ARM::t2LDRSB_PRE:
  case ARM::t2LDRSB_POST:
    //  case ARM::LDRSB_PRE:
    //  case ARM::LDRSB_POST:
    return instLDRimm(Opcode, ARM::t2LDRSBT, MI, MFI, TII);
    //LDRSB (literal) skip
    //LDRSB (register)
  case ARM::t2LDRSBs:
    return instLDRreg(Opcode, ARM::t2LDRSBT, MI, MFI, TII);


    //[TODO] load double, load multiple, float load
    //LDRD
    //  case ARM::LDRD:
  case ARM::t2LDRDi8:

    //VLDR
  case ARM::VLDRS:
  case ARM::VLDRD:

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
  // MRI = &Fn.getRegInfo();
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
    for (MachineInstr &MI : MFI) {
      if (!instMemoryOp(MI, MFI, TII)) continue;
	count++;
        //        dbgs() << "instrumented !!!!! : " << count  << "\n";
    }
  }

  return Modified;
}

FunctionPass *llvm::createARMTestPassPass() {
  return new ARMTestPass();
}
