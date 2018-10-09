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
    MachineRegisterInfo *MRI;
    // MachineFunction *MF;

	int count = 0;

    void addOffsetInst(MachineInstr &MI, MachineBasicBlock &MFI,
                       unsigned insttype, unsigned dest_reg,
                       unsigned reg1, unsigned reg2,
                       int imm, int shift_imm);

    bool instLDRreg(unsigned Opcode, unsigned new_opcode, MachineInstr &MI,
                    MachineBasicBlock &MFI);
    bool instLDRimm(unsigned Opcode, unsigned new_opcode,
                    MachineInstr &MI, MachineBasicBlock &MFI);
    bool instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI);

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

void ARMTestPass::addOffsetInst(MachineInstr &MI, MachineBasicBlock &MFI,
                                unsigned insttype, unsigned dest_reg,
                                unsigned reg1, unsigned reg2,
                                int imm, int shift_imm) {
  if(insttype == INST_ADD){
    AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDri12), dest_reg)
                   .addReg(reg1).addImm(imm));
  }else if(insttype == INST_SUB){
    AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2SUBri12), dest_reg)
                   .addReg(reg1).addImm(imm));
  }else if(insttype == INST_ADD_REG){
    AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tADDrr), dest_reg)
                   .addReg(ARM::CPSR, RegState::Implicit).addReg(reg1).addReg(reg2));
  }else if(insttype == INST_ADD_REG_IMM){
     // add.w rd, rn, rm lsl #imm
    AddDefaultCC(AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2ADDrs), dest_reg)
                                .addReg(reg1, RegState::Kill).addReg(reg2)
                                .addImm(ARM_AM::getSORegOpc(ARM_AM::lsl, shift_imm))));
  }
}

bool ARMTestPass::instLDRreg(unsigned Opcode, unsigned new_opcode,
                             MachineInstr &MI, MachineBasicBlock &MFI) {
  unsigned dest_reg = MI.getOperand(0).getReg();
  unsigned base_reg = MI.getOperand(1).getReg();
  unsigned offset_reg = MI.getOperand(2).getReg();

  unsigned new_inst = INST_NONE;
  int new_inst_imm = 0;
  int shift_imm = 0;

  if(isLdrRegT1Encoding(Opcode)){
    // LDR<c> <Rt>,[<Rn>,<Rm>]
    dbgs() << "T1 encoding for ldr (reg) case\n";
    new_inst = INST_ADD_REG;
  }else if(isLdrRegT2Encoding(Opcode)){
    // LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}]
    dbgs() << "T2 encoding for ldr (reg) case\n";
    new_inst = INST_ADD_REG_IMM;
    shift_imm = MI.getOperand(3).getImm();
  }else{
    return false;
  }

  if(new_inst != INST_NONE) {
    unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
    addOffsetInst(MI, MFI, new_inst, tmp_reg, base_reg, offset_reg, new_inst_imm,
                  shift_imm);
    base_reg = tmp_reg;
  }

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode), dest_reg)
                 .addReg(base_reg).addImm(0));

  return true;
}

bool ARMTestPass::instLDRimm(unsigned Opcode, unsigned new_opcode,
                             MachineInstr &MI, MachineBasicBlock &MFI) {
  unsigned dest_reg = MI.getOperand(0).getReg();
  unsigned dest_reg2 = -1; // pre/post index
  unsigned base_reg = -1;

  unsigned new_inst = INST_NONE;
  int new_inst_imm = 0;
  int offset_imm = 0;
  bool post_idx = false;

  if(isT1Encoding(Opcode)){
    //Encoding T1 : LDR<c> <Rt>, [<Rn>{,#<imm5>}]
    dbgs() << "T1 encoding ldr (imm) case\n";
    base_reg = MI.getOperand(1).getReg();
    offset_imm = transT1Imm(Opcode, MI.getOperand(2).getImm());
  }else if(isT3Encoding(Opcode)){
    //Encoding T3 : LDR<c>.W <Rt>,[<Rn>{,#<imm12>}]
    dbgs() << "T3 encoding ldr (imm) case\n";
    base_reg = MI.getOperand(1).getReg();
    new_inst = INST_ADD;
    new_inst_imm = MI.getOperand(2).getImm();
  }else if(isT4Encoding(Opcode)){
    //Encoding T4 : LDR<c> <Rt>,[<Rn>,#-<imm8>]
    int orig_imm;
    if(isT4PreEncoding(Opcode)){
      dbgs() << "T4 pre-index encoding ldr (imm) case\n";
      dest_reg2 = MI.getOperand(1).getReg();
      base_reg = MI.getOperand(2).getReg();
      orig_imm = MI.getOperand(3).getImm();
    }else if(isT4PostEncoding(Opcode)){
      dbgs() << "T4 post-index encoding ldr (imm) case\n";
      dest_reg2 = MI.getOperand(1).getReg();
      base_reg = MI.getOperand(2).getReg();
      orig_imm = MI.getOperand(3).getImm();
      post_idx = true;
    }else{
      dbgs() << "T4 encoding ldr (imm) case\n";
      base_reg = MI.getOperand(1).getReg();
      orig_imm = MI.getOperand(2).getImm();
    }

    if (orig_imm < 0) {
      new_inst = INST_SUB;
      new_inst_imm = -orig_imm;
    } else {
      new_inst = INST_ADD;
      new_inst_imm = orig_imm;
    }
  }else{
    return false;
  }

  if(new_inst != INST_NONE) {
    if (!post_idx) {
      unsigned tmp_reg = MRI->createVirtualRegister(&ARM::rGPRRegClass);
      addOffsetInst(MI, MFI, new_inst, tmp_reg, base_reg, 0, new_inst_imm, 0);
      base_reg = tmp_reg;
    } else {
      addOffsetInst(MI, MFI, new_inst, dest_reg2, base_reg, 0, new_inst_imm, 0);
    }
  }

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode), dest_reg)
               .addReg(base_reg).addImm(offset_imm));

  return true;
}

bool ARMTestPass::instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI) {
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
    return instLDRimm(Opcode, ARM::t2LDRT, MI, MFI);
    //LDR (literal) skip
    //LDR (register)
    //  case ARM::LDRrs:
  case ARM::tLDRr:
  case ARM::t2LDRs:         //instrumented but not checked
    //    return false;
    return instLDRreg(Opcode, ARM::t2LDRT, MI, MFI);

    //LDRH (immediate)
    //  case ARM::LDRH:           //not used??
  case ARM::tLDRHi:
  case ARM::t2LDRHi12:
  case ARM::t2LDRHi8:
  case ARM::t2LDRH_PRE:
  case ARM::t2LDRH_POST:
    //  case ARM::LDRH_PRE:
    //  case ARM::LDRH_POST:
    return instLDRimm(Opcode, ARM::t2LDRHT, MI, MFI);
    //LDRH (litenal) skip
    //LDRH (register)
  case ARM::t2LDRHs:
  case ARM::tLDRHr:
    return instLDRreg(Opcode, ARM::t2LDRHT, MI, MFI);

    //LDRB (immediate)
    //  case ARM::LDRBi12:        //not used??
  case ARM::tLDRBi:
  case ARM::t2LDRBi12:
  case ARM::t2LDRBi8:
  case ARM::t2LDRB_PRE:
  case ARM::t2LDRB_POST:
    //  case ARM::LDRB_PRE_IMM:
    //  case ARM::LDRB_POST_IMM:
    return instLDRimm(Opcode, ARM::t2LDRBT, MI, MFI);
    //LDRB (literal) skip
    //LDRB (register)
  case ARM::t2LDRBs:
  case ARM::tLDRBr:
    return instLDRreg(Opcode, ARM::t2LDRBT, MI, MFI);

    //LDRSH (immediate)
    //  case ARM::LDRSH:          //not used??
  case ARM::tLDRSH:               //not used??
  case ARM::t2LDRSHi12:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSH_PRE:
  case ARM::t2LDRSH_POST:
    //  case ARM::LDRSH_PRE:
    //  case ARM::LDRSH_POST:
    return instLDRimm(Opcode, ARM::t2LDRSHT, MI, MFI);
    //LDRSH (litenal) skip
    //LDRSH (register)
 case ARM::t2LDRSHs:
   return instLDRreg(Opcode, ARM::t2LDRSHT, MI, MFI);

    //LDRSB (immediate)
   //  case ARM::LDRSB:           //not used??
  case ARM::tLDRSB:
  case ARM::t2LDRSBi12:
  case ARM::t2LDRSBi8:
  case ARM::t2LDRSB_PRE:
  case ARM::t2LDRSB_POST:
    //  case ARM::LDRSB_PRE:
    //  case ARM::LDRSB_POST:
    return instLDRimm(Opcode, ARM::t2LDRSBT, MI, MFI);
    //LDRSB (literal) skip
    //LDRSB (register)
  case ARM::t2LDRSBs:
    return instLDRreg(Opcode, ARM::t2LDRSBT, MI, MFI);


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
