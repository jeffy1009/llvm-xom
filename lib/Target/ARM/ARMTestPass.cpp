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
#define INST_ADD_REG 6
#define INST_LSL 2
#define INST_SUB 3
#define INST_SUB_REG 5
#define INST_LSR 4


static void addOffsetInst(MachineInstr &MI, MachineBasicBlock &MFI,
                          const TargetInstrInfo* TII, unsigned insttype,
                          unsigned imm, unsigned imm2, unsigned reg1, unsigned reg2) {
  if(insttype == INST_ADD){
    //    insttype = ARM::tADDi8; // working
    //    insttype = ARM::tMOVr;
    //    insttype = ARM::tLSLri;
    insttype = ARM::t2ADDri12;

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(insttype))
    .addReg(reg1, RegState::Define)
    .addReg(reg1, RegState::Kill)
    .addImm(imm2));

  }
  else if(insttype == INST_SUB){
    //    insttype = ARM::tSUBi8; // working
    //    insttype = ARM::tMOVr;
    //    insttype = ARM::tLSRri;
    insttype = ARM::t2SUBri12;

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(insttype))
    .addReg(reg1, RegState::Define)
    .addReg(reg1, RegState::Kill)
    .addImm(imm2));

  }else if(insttype == INST_ADD_REG){

    //    insttype = ARM::ADDrr;

    //     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(insttype))
    //     .addReg(reg1, RegState::Define)
    //     .addReg(reg1)
    //     .addReg(reg2)
    //     .addImm(imm2));

     // mov.w rm, rm, lsl #imm
     // adds rn, rn, rm

     AddDefaultCC(AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::t2LSLri))
                                 .addReg(reg2, RegState::Define)
                                 .addReg(reg2).addImm(imm2)));

     AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(ARM::tADDrr))
                    .addReg(reg1, RegState::Define)
                    .addReg(ARM::CPSR, RegState::Define)
                    .addReg(reg1).addReg(reg2));

  }else if(insttype == INST_SUB_REG){
    insttype == ARM::SUBrr;

    //  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(insttype))
    //   .addReg(reg1, RegState::Define)
    //   .addReg(reg1)
    //   .addReg(reg2)
    //   .addImm(imm2));

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

static bool instLDRreg(unsigned Opcode, MachineInstr &MI,
                       MachineBasicBlock &MFI, const TargetInstrInfo* TII){
  unsigned reg0 = MI.getOperand(0).getReg();
  unsigned reg1 = MI.getOperand(1).getReg();
  unsigned reg2 = MI.getOperand(2).getReg();
  unsigned imm = MI.getOperand(3).getImm();
  unsigned new_opcode = ARM::t2LDRi12;
  //unsigned new_opcode = ARM::t2LDRT;
  unsigned pre_inst = INST_NONE;
  unsigned post_inst = INST_NONE;
  unsigned imm_inst = INST_NONE;

  if(Opcode == ARM::LDRrs){
    dbgs() << "LDRrs\n";
  }else if(Opcode == ARM::tLDRr){
        dbgs() << "tLDRr\n";
  }else if(Opcode == ARM::t2LDRs){
        dbgs() << "t2LDRs\n";
        //        LDR<c>.W <Rt>,[<Rn>,<Rm>{,LSL #<imm2>}]
        pre_inst = INST_ADD_REG;
        post_inst = INST_SUB_REG;
        imm_inst = imm;
        imm = 0;
  }else{
    return false;
  }

  if(pre_inst != INST_NONE) addOffsetInst(MI, MFI, TII, pre_inst, imm, imm_inst, reg1, reg2);


  //  dbgs() << "instrumented!!! \n";
  //  MFI.erase(MI);

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
                 .addReg(reg0, RegState::Define)
                 .addReg(reg1)
                 .addImm(imm));

  //  MFI.erase(MI);
  //  MI.eraseFromParent();

  if(post_inst != INST_NONE) addOffsetInst(MI, MFI, TII, post_inst, imm, imm_inst, reg1, reg2);

  return true;
}

static bool instLDRimm(unsigned Opcode, MachineInstr &MI,
                       MachineBasicBlock &MFI, const TargetInstrInfo* TII){
  unsigned reg0 = MI.getOperand(0).getReg();
  unsigned reg1 = MI.getOperand(1).getReg();
  unsigned reg2 = 0;
  unsigned imm = MI.getOperand(2).getImm();
  unsigned new_opcode = ARM::t2LDRi12;
  //unsigned new_opcode = ARM::t2LDRT;
  unsigned pre_inst = INST_NONE;
  unsigned post_inst = INST_NONE;
  unsigned imm_inst = INST_NONE;

  if(Opcode == ARM::tLDRi){
    //Encoding T1 : LDR<c> <Rt>, [<Rn>{,#<imm5>}]
    //i32 = ZeroExtend(imm5:00, 32); just shift 2bit
    dbgs() << "tLDRi\n";
    imm = imm << 2;
  }else if(Opcode == ARM::t2LDRi8){
    //Encoding T4 : LDR<c> <Rt>,[<Rn>,#-<imm8>]
    //[TODO] negative imm8 case!!
    dbgs() << "t2LDRi8\n";
  }else if(Opcode == ARM::t2LDRi12 || Opcode == ARM::LDRi12){
    //Encoding T3 : LDR<c>.W <Rt>,[<Rn>{,#<imm12>}]
        dbgs() << "t2LDRi12 or LDRi12\n";
    pre_inst = INST_ADD;
    post_inst = INST_SUB;
    imm_inst = imm;
    imm = 0;
  }else if(Opcode == ARM::LDR_PRE_IMM){
    //Encoding T4 : LDR<c> <Rt>,[<Rn>,#+/-<imm8>]!
    //[TODO] negative imm cass!!
        dbgs() << "LDR_PRE_IMM\n";
    pre_inst = INST_ADD;
    post_inst = INST_NONE;
    imm_inst = imm;
    imm = 0;
  }else if(Opcode == ARM::LDR_POST_IMM){
    //Encoding T4 : LDR<c> <Rt>,[<Rn>],#+/-<imm8>
    //[TODO] negative imm case!!
        dbgs() << "LDR_POST_IMM\n";
    pre_inst = INST_NONE;
    post_inst = INST_ADD;
    imm_inst = imm;
    imm = 0;
  }else{
    return false;
  }


  if(pre_inst != INST_NONE) addOffsetInst(MI, MFI, TII, pre_inst, imm, imm_inst, reg1, reg2);


  //  dbgs() << "instrumented!!! \n";
  //  MFI.erase(MI);

  AddDefaultPred(BuildMI(MFI, &MI, MI.getDebugLoc(), TII->get(new_opcode))
                 .addReg(reg0, RegState::Define)
                 .addReg(reg1)
                 .addImm(imm));

  //  MFI.erase(MI);
  //  MI.eraseFromParent();

  if(post_inst != INST_NONE) addOffsetInst(MI, MFI, TII, post_inst, imm, imm_inst, reg1, reg2);


  //  MFI.erase(MI);
  return true;
}

/// Returns true if instruction is a memory operation that this pass is capable
/// of operating on.
static bool instMemoryOp(MachineInstr &MI, MachineBasicBlock &MFI,
                         const TargetInstrInfo* TII) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {

    //LDR (immediate)
    /  case ARM::tLDRi:
    //  case ARM::tLDRspi:  //stack base load skip
      case ARM::t2LDRi8:
      case ARM::t2LDRi12:
      case ARM::LDRi12:
    //      return true;
           return instLDRimm(Opcode, MI, MFI, TII);
    //LDR (literal) skip

    //LDR (register)
         case ARM::LDRrs:
         case ARM::tLDRr:
         case ARM::t2LDRs:
   /  return instLDRreg(Opcode, MI, MFI, TII);

    //LDRH (immediate)
  case ARM::LDRH:
  case ARM::tLDRHi:
  case ARM::t2LDRHi8:
  case ARM::t2LDRHi12:
    //LDRH (litenal) skip
    //LDRH (register)
  case ARM::t2LDRHs:
  case ARM::tLDRHr:

    //LDRB (immediate)
  case ARM::LDRBi12:
  case ARM::tLDRBi:
  case ARM::t2LDRBi8:
  case ARM::t2LDRBi12:
    //LDRB (literal) skip
    //LDRB (register)
  case ARM::t2LDRBs:
  case ARM::tLDRBr:

    //LDRSH (immediate)
  case ARM::LDRSH:
  case ARM::tLDRSH:
  case ARM::t2LDRSHi8:
  case ARM::t2LDRSHi12:
    //LDRSH (litenal) skip
    //LDRSH (register)
 case ARM::t2LDRSHs:

    //LDRSB (immediate)
  case ARM::LDRSB:
  case ARM::tLDRSB:
  case ARM::t2LDRSBi8:
  case ARM::t2LDRSBi12:
    //LDRSB (literal) skip
    //LDRSB (register)
  case ARM::t2LDRSBs:

    //LDRD
  case ARM::LDRD:
  case ARM::t2LDRDi8:

    //VLDR
  case ARM::VLDRS:
  case ARM::VLDRD:

    break;
  default:
    return false;
  }
  return false;
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
//  return true;
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
    //    if(Fn.getName().compare(StringRef("sort")) != 0){
    //      dbgs() << "Current function name: [" << Fn.getName() << "]\n";
    //      continue;
    //    }

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
