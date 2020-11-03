#include <fstream>
#include "ARM.h"
#include "ARMSubtarget.h"
#include "MCTargetDesc/ARMAddressingModes.h"
#include "Thumb2InstrInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "arm-testpass"

namespace llvm {
void initializeARMRemoveUnintendedPass(PassRegistry &);
}

#define ARM_TESTPASS_NAME "ARM remove unintended"

cl::opt<std::string>
UnintendedFilePath("unintended-file-path", cl::ValueOptional, cl::ZeroOrMore,
                   cl::desc("filepath of unintended.txt"), cl::init("."));

namespace {
  struct ARMRemoveUnintended : public MachineFunctionPass{
    static char ID;
    const ARMSubtarget *STI;
    const TargetInstrInfo *TII;
    const MachineRegisterInfo *MRI;

    ARMRemoveUnintended() : MachineFunctionPass(ID) {
      initializeARMRemoveUnintendedPass(*PassRegistry::getPassRegistry());
    }

    bool runOnMachineFunction(MachineFunction &Fn) override;

    StringRef getPassName() const override {
      return ARM_TESTPASS_NAME;
    }

  };
  char ARMRemoveUnintended::ID = 0;
}

INITIALIZE_PASS(ARMRemoveUnintended, "arm-remove-unintended", ARM_TESTPASS_NAME, false, false)

static bool isRegDangerous(unsigned Reg) {
  return Reg==ARM::R5 || Reg==ARM::R6 || Reg==ARM::R7 || Reg==ARM::R8 || Reg==ARM::R11 || Reg==ARM::R12;
}

static unsigned findScratchReg(MachineFunction &Fn, MachineBasicBlock &MFI, MachineInstr &MI,
                               bool No5678 = false) {
  const MachineRegisterInfo *MRI = &Fn.getRegInfo();
  const TargetRegisterInfo *TRI = MRI->getTargetRegisterInfo();

  // Don't know if this is safe, but we need this to use LivePhysRegs
  Fn.getProperties().set(MachineFunctionProperties::Property::TracksLiveness);

  LivePhysRegs LiveRegs;
  LiveRegs.init(*TRI);
  LiveRegs.addLiveOuts(MFI);
  for (MachineInstr &TmpMI : make_range(MFI.rbegin(), std::next(MachineBasicBlock::reverse_iterator(MI))))
    LiveRegs.stepBackward(TmpMI);
  for (unsigned ScratchReg : ARM::rGPRRegClass) {
    if (No5678 && isRegDangerous(ScratchReg))
      continue;
    if (!LiveRegs.available(*MRI, ScratchReg)) continue;
    return ScratchReg;
  }
  return 0;
}

/// Returns true if instruction \p MI will not result in actual machine code
/// instructions.
static bool doesNotGeneratecode(MachineInstr &MI) {
  // TODO: Introduce an MCInstrDesc flag for this
  switch (MI.getOpcode()) {
  default: return false;
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::CFI_INSTRUCTION:
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::GC_LABEL:
  case TargetOpcode::DBG_VALUE:
    return true;
  }
}

struct UnintendedInfo {
  char mnemonic[3];
  UnintendedInfo() { mnemonic[0] = '\0'; }
  UnintendedInfo(StringRef Str) {
    mnemonic[0] = Str[0];
    mnemonic[1] = Str[1];
    mnemonic[2] = '\0';
  }
};

bool ARMRemoveUnintended::runOnMachineFunction(MachineFunction &Fn) {
  // We don't handle assembly function
  if (Fn.getFunction()->hasFnAttribute(Attribute::Naked))
    return false;

  STI = &static_cast<const ARMSubtarget &>(Fn.getSubtarget());
  TII = STI->getInstrInfo();
  MRI = &Fn.getRegInfo();

  bool Modified = false;

  int Trial = 0;
 restart:
  std::string Filepath;
  Filepath.append(UnintendedFilePath).append("/unintended");

  if (Trial > 0)
    Filepath.append("2");
  Filepath.append(".txt");

  std::ifstream In(Filepath);
  if (!In.good()) {
    llvm::errs() << "WARNING: couldn't load file '" << Filepath <<"'\n";
    return Modified;
  }

  DenseMap<int, UnintendedInfo> Indices;
  std::string Line;
  int CaseNum;
  int CaseArr[3] = {0};
  while (In) {
    std::getline(In, Line);
    StringRef LineRef(Line);
    if (!LineRef.startswith("case"))
      continue;

    int col1_pos = LineRef.find(':');
    int col2_pos = LineRef.find(':', col1_pos+1);
    if (!Fn.getName().equals(LineRef.slice(col1_pos+3, col2_pos-1)))
      continue;

    bool Failed = LineRef.substr(std::strlen("case"), 1).getAsInteger(10, CaseNum);
    assert(!Failed);

    int index;
    Failed = LineRef.drop_front(col2_pos+2).getAsInteger(10, index);
    assert(!Failed);
    CaseArr[CaseNum-1]++;

    std::getline(In, Line);
    LineRef = StringRef(Line);
    int braket_pos = LineRef.find('[');
    int mnemonic_pos = LineRef.find('u', braket_pos+2)+2;
    Indices[index] = LineRef.substr(mnemonic_pos, 2);
  }

  if (Indices.empty())
    return Modified;

  dbgs() << "Found unintended insts in function:" << Fn.getName() << '\n';
  dbgs() << "Case1: " << CaseArr[0] << " Case2: " << CaseArr[1] << " Case3: " << CaseArr[2] << '\n';

  int CurIndex = 0;
  for (MachineBasicBlock &MFI : Fn) {
    MachineBasicBlock::iterator MBBI = MFI.begin(), MBBIE = MFI.end();
    bool FixITBlock = false;
    bool FixITNow = false;
    MachineInstr* IT = nullptr;
    MachineInstr *LastIT = nullptr;
    while (MBBI != MBBIE) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      MachineInstr &MI = *MBBI;
      MBBI = NMBBI;

      if (doesNotGeneratecode(MI))
        continue;

      switch (MI.getOpcode()) {
      case TargetOpcode::INLINEASM: {
        const MCAsmInfo *MAI = Fn.getTarget().getMCAsmInfo();
        unsigned AsmSize = TII->getInlineAsmLength(MI.getOperand(0).getSymbolName(), *MAI);
        CurIndex += AsmSize/MAI->getMaxInstLength();
        assert(Fn.getName().equals("pm_reboot") || Fn.getName().equals("pm_set_lowest"));
        continue;
      }
      case TargetOpcode::LOCAL_ESCAPE:
        assert(0);
      default: break;
      }

      bool KillsITSTATE = false;
      if (MI.killsRegister(ARM::ITSTATE))
        KillsITSTATE = true;

      if (Indices.count(CurIndex)) {
        char *mnemonic = Indices[CurIndex].mnemonic;
        if (!TII->getName(MI.getOpcode()).contains_lower(mnemonic[0])) {
          // Definitely not the right file
          dbgs() << "WARNING: Skipping function with mismatches\n";
          return Modified;
        }

        if (!TII->getName(MI.getOpcode()).contains_lower(mnemonic)) {
          if (mnemonic[0]!='b') { // t2Bcc <-> blt, ...
            // This is probably the wrong file. Bail out.
            // This happens when compiling the same function in newlib and newlib-nano
            dbgs() << "WARNING: Skipping function with mismatches\n";
            return Modified;
          }
        }

        unsigned PredReg;
        ARMCC::CondCodes PredCC = getInstrPredicate(MI, PredReg);
        std::array<MachineOperand, 2> Pred = predOps(PredCC, PredReg);
        bool InsideIT = false;
        unsigned ITDist = 0;
        int ITCount = 0;
        if (MI.readsRegister(ARM::ITSTATE)) {
          InsideIT = true;
          // check # of dependent insts
          auto TmpMII = std::prev(MI.getIterator());
          while (true) {
            if (TmpMII->getOpcode() == ARM::t2IT) break;
            if (!doesNotGeneratecode(*TmpMII)) {
              ++ITCount; ++ITDist;
            }
            --TmpMII;
            // assert(ITCount<4);
          }
          IT = &*TmpMII;
          TmpMII = MI.getIterator();
          while (true) {
            if (!doesNotGeneratecode(*TmpMII)) {
              if (TmpMII->killsRegister(ARM::ITSTATE)) break;
              ++ITCount;
            }
            ++TmpMII;
            // assert(ITCount<4);
          }
          ++ITCount;
        }

        // if (FixITBlock && InsideIT)
        //   goto jump;

        Modified = true;

        bool Changed = false;
        switch (MI.getOpcode()) {
        case ARM::t2ANDrs:
        case ARM::t2ORRrs:
        case ARM::t2RSBrs:
        case ARM::t2ADDrs:
        case ARM::t2SUBrs:
        case ARM::t2CMPrs: {
          int Rd = (MI.getOpcode() != ARM::t2CMPrs) ? MI.getOperand(0).getReg() : 0;
          int Rn = (MI.getOpcode() != ARM::t2CMPrs) ? MI.getOperand(1).getReg() : MI.getOperand(0).getReg();
          int Rm = (MI.getOpcode() != ARM::t2CMPrs) ? MI.getOperand(2).getReg() : MI.getOperand(1).getReg();
          int imm =(MI.getOpcode() != ARM::t2CMPrs) ? MI.getOperand(3).getImm() : MI.getOperand(2).getImm();
          ARM_AM::ShiftOpc shiftmode = ARM_AM::getSORegShOp(imm);
          int shiftamt = ARM_AM::getSORegOffset(imm);
          int shiftamt1 = shiftamt;
          int shiftamt2 = 0;
          bool HasCC = (MI.getOperand(MI.getDesc().getNumOperands()-1).getReg() == ARM::CPSR);
          assert(!HasCC); // TODO: handle this case
          if (MI.getOpcode() == ARM::t2CMPrs) assert(!InsideIT);
          if (shiftamt > 16) {
            shiftamt1 = shiftamt - 16;
            shiftamt2 = shiftamt - shiftamt1;
          }

          // check if CPSR is live
          bool CPSRLive = false;
          if (!InsideIT) {
            auto TmpMII = MI.getIterator();
            while (true) {
              if (TmpMII == MFI.begin()) {
                if (MFI.isLiveIn(ARM::CPSR))
                  CPSRLive = true;
                break;
              }
              --TmpMII;
              if (TmpMII->killsRegister(ARM::CPSR) || TmpMII->registerDefIsDead(ARM::CPSR))
                break;
              if (TmpMII->definesRegister(ARM::CPSR)) {
                CPSRLive = true;
                break;
              }
            }
          }

          unsigned NewRd = Rd;
          if (Rd == 0 || Rd == Rn) // We need a new register
            NewRd = findScratchReg(Fn, MFI, MI);
          if (!NewRd) // FIXME
            break;

          MachineInstrBuilder MIB;
          if (NewRd < ARM::R8 && Rm < ARM::R8 && !CPSRLive) {
            int shiftopc;
            switch (shiftmode) {
            case ARM_AM::asr: shiftopc = ARM::tASRri; break;
            case ARM_AM::lsl: shiftopc = ARM::tLSLri; break;
            case ARM_AM::lsr: shiftopc = ARM::tLSRri; break;
            default: assert(0);
            }

            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(shiftopc), NewRd);

            if (InsideIT)
              MIB.add(condCodeOp());
            else
              MIB.addReg(ARM::CPSR, RegState::Define); // TODO Add Dead flag

            MIB.addReg(Rm).addImm(shiftamt1).add(Pred);
          } else {
            int shiftopc;
            switch (shiftmode) {
            case ARM_AM::asr: shiftopc = ARM::t2ASRri; break;
            case ARM_AM::lsl: shiftopc = ARM::t2LSLri; break;
            case ARM_AM::lsr: shiftopc = ARM::t2LSRri; break;
            default: assert(0);
            }

            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(shiftopc), NewRd)
              .addReg(Rm).addImm(shiftamt1).add(Pred).add(condCodeOp());
          }

          if (InsideIT)
            MIB.addReg(ARM::ITSTATE, RegState::Implicit);

          if (shiftamt <= 16 && !CPSRLive && (Rd < ARM::R8 && NewRd < ARM::R8 && Rn < ARM::R8)) {
            int new_opc;
            switch (MI.getOpcode()) {
            case ARM::t2ANDrs: new_opc = ARM::tAND; break;
            case ARM::t2ORRrs: new_opc = ARM::tORR; break;
            case ARM::t2RSBrs: new_opc = ARM::tRSB; break;
            case ARM::t2ADDrs: new_opc = ARM::tADDrr; break;
            case ARM::t2SUBrs: new_opc = ARM::tSUBrr; break;
            case ARM::t2CMPrs: new_opc = ARM::tCMPr; break;
            default: assert(0);
            }

            if (MI.getOpcode()!=ARM::t2CMPrs) {
              MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc), Rd);
              if (InsideIT)
                MIB.add(condCodeOp());
              else
                MIB.addReg(ARM::CPSR, RegState::Define); // TODO Add Dead flag

              MIB.addReg(NewRd).addReg(Rn).add(Pred);
            } else {
              MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc))
                                                   .addReg(Rn).addReg(NewRd).add(Pred);
            }

            // switch (MI.getOpcode()) {
            // case ARM::t2ANDrs: new_opc = ARM::t2ANDrr; break;
            // case ARM::t2ORRrs: new_opc = ARM::t2ORRrr; break;
            // default: assert(0);
            // }
            // MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc), Rd)
            //   .addReg(Rn).addReg(NewRd).add(Pred).add(condCodeOp());
          } else {
            if (MI.getOpcode()!=ARM::t2CMPrs) {
              MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(MI.getOpcode()), Rd)
                .addReg(Rn).addReg(NewRd, RegState::Kill)
                .addImm(ARM_AM::getSORegOpc(shiftmode, shiftamt2))
                .add(Pred).add(condCodeOp());
            } else {
              MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(MI.getOpcode()))
                .addReg(Rn).addReg(NewRd, RegState::Kill)
                .addImm(ARM_AM::getSORegOpc(shiftmode, shiftamt2))
                .add(Pred);
            }
          }

          if (InsideIT)
            MIB.addReg(ARM::ITSTATE,
                       RegState::Implicit | (MI.killsRegister(ARM::ITSTATE)?RegState::Kill:0));

          MI.eraseFromParent();
          Changed = true;
          break;
        }

        case ARM::t2MOVi16: {
          MachineInstrBuilder MIB;
          if (MI.getOperand(1).isImm()) {
            unsigned Rd = MI.getOperand(0).getReg();
            unsigned orig_imm = MI.getOperand(1).getImm();
            assert(orig_imm & 0x0400);

            unsigned addend = 0x0400;
            unsigned new_imm = orig_imm - 0x0400;
            MI.getOperand(1).setImm(new_imm);

            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2ADDri12), Rd)
              .addReg(Rd).addImm(addend).add(Pred);
          } else {
            assert(MI.getOperand(1).isGlobal());
            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2HINT))
              .addImm(0).add(Pred);
          }

          if (InsideIT) {
            MIB.addReg(ARM::ITSTATE,
                       RegState::Implicit | (MI.killsRegister(ARM::ITSTATE)?RegState::Kill:0));
            MI.findRegisterUseOperand(ARM::ITSTATE)->setIsKill(false);
          }
          Changed = true;
          break;
        }

        case ARM::t2MOVTi16: {
          MachineInstrBuilder MIB;
          if (MI.getOperand(2).isImm()) {
            unsigned Rd = MI.getOperand(0).getReg();
            unsigned orig_imm = MI.getOperand(2).getImm();
            assert(orig_imm & 0x0400);

            // FIXME: this has some error
            unsigned new_imm = (~orig_imm & 0x8000) | (orig_imm & 0x7fff) & 0xfbff;
            unsigned xor_imm = 0x84000000;
            MI.getOperand(2).setImm(new_imm);

            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2EORri), Rd)
              .addReg(Rd).addImm(xor_imm).add(Pred).add(condCodeOp());
            // MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2HINT))
            //   .addImm(0).add(Pred);
          } else {
            assert(MI.getOperand(2).isGlobal());
            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2HINT))
              .addImm(0).add(Pred);
          }

          if (InsideIT) {
            MIB.addReg(ARM::ITSTATE,
                       RegState::Implicit | (MI.killsRegister(ARM::ITSTATE)?RegState::Kill:0));
            MI.findRegisterUseOperand(ARM::ITSTATE)->setIsKill(false);
          }
          Changed = true;
          break;
        }

        case ARM::t2ADDri12: {
          MachineInstrBuilder MIB;
          unsigned Rd = MI.getOperand(0).getReg();
          unsigned orig_imm = MI.getOperand(2).getImm();
          assert(orig_imm & 0x0400);
          unsigned addend = 0x0400;
          unsigned new_imm = orig_imm - 0x0400;

          MI.getOperand(2).setImm(new_imm);
          MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2ADDri12), Rd)
            .addReg(Rd).addImm(addend).add(Pred);
          assert(!InsideIT);
          Changed = true;
          break;
        }

        case ARM::t2ASRri:
        case ARM::t2LSLri:
        case ARM::t2LSRri:{
          MachineInstrBuilder MIB;
          unsigned Rd = MI.getOperand(0).getReg();
          unsigned orig_imm = MI.getOperand(2).getImm();
          assert(orig_imm > 16);
          unsigned addend = 16;
          unsigned new_imm = orig_imm - 16;

          bool HasCC = (MI.getOperand(MI.getDesc().getNumOperands()-1).getReg() == ARM::CPSR);

          MI.getOperand(2).setImm(new_imm);
          MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(MI.getOpcode()), Rd)
            .addReg(Rd).addImm(addend).add(Pred);

          if (HasCC)
            MIB.addReg(ARM::CPSR, RegState::Define);
          else
            MIB.add(condCodeOp());

          assert(!InsideIT);
          Changed = true;
          break;
        }

        case ARM::t2MOVi: // TODO: optimize t2MOVi => t2MOVi16
        case ARM::t2ADDri:
        case ARM::t2ANDri:
        case ARM::t2BICri:
        case ARM::t2EORri:
        case ARM::t2ORRri:
        case ARM::t2SUBri: {
          MachineInstrBuilder MIB;
          unsigned Rd = MI.getOperand(0).getReg();
          unsigned Rn = 0;
          if (MI.getOpcode()!=ARM::t2MOVi)
            Rn = MI.getOperand(1).getReg();
          unsigned Tmp = Rd;
          if (Rd == Rn)
            Tmp = findScratchReg(Fn, MFI, MI);
          if (!Tmp) // FIXME
            break;

          bool HasCC = (MI.getOperand(MI.getDesc().getNumOperands()-1).getReg() == ARM::CPSR);
          assert(!HasCC); // TODO: handle this case

          unsigned orig_imm;
          if (MI.getOpcode()!=ARM::t2MOVi)
            orig_imm = MI.getOperand(2).getImm();
          else
            orig_imm = MI.getOperand(1).getImm();
          if (ARM_AM::getT2SOImmValRotateVal(orig_imm)==-1) break; // FIXME
          unsigned RotAmt = countLeadingZeros(orig_imm);
          unsigned shift, new_imm;
          ARM_AM::ShiftOpc shop;
          if (RotAmt>9) {
            shift = (24-RotAmt);
            new_imm = orig_imm >> shift;
            shop = ARM_AM::lsl;
          } else {
            shift = (RotAmt+8);
            new_imm = orig_imm >> (32-shift);
            shop = ARM_AM::ror;
          }

          int new_opc;
          switch (MI.getOpcode()) {
          case ARM::t2MOVi: new_opc = (shop==ARM_AM::lsl ? ARM::t2LSLri : ARM::t2RORri); break;
          case ARM::t2ADDri: new_opc = ARM::t2ADDrs; break;
          case ARM::t2ANDri: new_opc = ARM::t2ANDrs; break;
          case ARM::t2BICri: new_opc = ARM::t2BICrs; break;
          case ARM::t2EORri: new_opc = ARM::t2EORrs; break;
          case ARM::t2ORRri: new_opc = ARM::t2ORRrs; break;
          case ARM::t2SUBri: new_opc = ARM::t2SUBrs; break;
          default: assert(0);
          }

          // TODO: t2MOVi => tMOVi minor optimization
          MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2MOVi), Tmp)
            .addImm(new_imm).add(Pred).add(condCodeOp());

          if (InsideIT)
            MIB.addReg(ARM::ITSTATE, RegState::Implicit);

          if (MI.getOpcode()!=ARM::t2MOVi)
            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc), Rd)
              .addReg(Rn).addReg(Tmp, RegState::Kill)
              .addImm(ARM_AM::getSORegOpc(shop, shift))
              .add(Pred).add(condCodeOp());
          else
            MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc), Rd)
              .addReg(Rd).addImm(shift).add(Pred).add(condCodeOp());

          if (InsideIT)
            MIB.addReg(ARM::ITSTATE,
                       RegState::Implicit | (MI.killsRegister(ARM::ITSTATE)?RegState::Kill:0));

          MI.eraseFromParent();
          Changed = true;
          break;
        }

        case ARM::t2CMPri:
        case ARM::t2TSTri: {
          MachineInstrBuilder MIB;
          unsigned Rn = MI.getOperand(0).getReg();
          unsigned Tmp = findScratchReg(Fn, MFI, MI);
          if (!Tmp) // FIXME
            break;

          int new_opc;
          switch (MI.getOpcode()) {
          case ARM::t2CMPri: new_opc = ARM::t2CMPrs; break;
          case ARM::t2TSTri: new_opc = ARM::t2TSTrs; break;
          default: assert(0);
          }

          unsigned orig_imm = MI.getOperand(1).getImm();
          if (ARM_AM::getT2SOImmValRotateVal(orig_imm)==-1) break; // FIXME
          unsigned RotAmt = countLeadingZeros(orig_imm);
          unsigned shift, new_imm;
          ARM_AM::ShiftOpc shop;
          if (RotAmt>9) {
            shift = (24-RotAmt);
            new_imm = orig_imm >> shift;
            shop = ARM_AM::lsl;
          } else {
            shift = (RotAmt+8);
            new_imm = orig_imm >> (32-shift);
            shop = ARM_AM::ror;
          }

          // assert(Tmp < ARM::R8);
          MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2MOVi), Tmp)
            .addImm(new_imm).add(Pred).add(condCodeOp());
          // MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::tMOVi8), Tmp)
          //   .add(condCodeOp()).addImm(new_imm).add(Pred);
          MIB = BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(new_opc))
            .addReg(Rn).addReg(Tmp, RegState::Kill)
            .addImm(ARM_AM::getSORegOpc(shop, shift))
            .add(Pred).addReg(ARM::CPSR, RegState::Define|RegState::Implicit);

          MI.eraseFromParent();
          Changed = true;
          assert(!InsideIT);
          break;
        }

        case ARM::t2Bcc: {
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2IT))
            .addImm(PredCC).addImm(0x18);
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::tB))
            .add(MI.getOperand(0)).add(Pred).addReg(ARM::ITSTATE, RegState::Implicit|RegState::Kill);

          MI.eraseFromParent();
          Changed = true;
          assert(!InsideIT);
          break;
        }

        case ARM::tBL: {
          assert(MI.getOperand(1).getReg()!=ARM::CPSR);
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2ADDri), ARM::LR)
            .addReg(ARM::PC).addImm(5).add(Pred).add(condCodeOp());
          BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::t2B))
            .add(MI.getOperand(2)).add(predOps(ARMCC::AL));

          MI.eraseFromParent();
          Changed = true;
          assert(!InsideIT);
          break;
        }

        case ARM::t2LDRi12:   case ARM::t2LDRi8:
        case ARM::t2LDRHi12:  case ARM::t2LDRHi8:
        case ARM::t2LDRBi12:  case ARM::t2LDRBi8:
        case ARM::t2LDRSHi12: case ARM::t2LDRSHi8:
        case ARM::t2LDRSBi12: case ARM::t2LDRSBi8:
        case ARM::t2LDR_PRE:   case ARM::t2LDR_POST:
        case ARM::t2LDRH_PRE:  case ARM::t2LDRH_POST:
        case ARM::t2LDRB_PRE:  case ARM::t2LDRB_POST:
        case ARM::t2LDRSH_PRE: case ARM::t2LDRSH_POST:
        case ARM::t2LDRSB_PRE: case ARM::t2LDRSB_POST:
        case ARM::t2LDRT:
        case ARM::t2LDRHT: case ARM::t2LDRSHT:
        case ARM::t2LDRBT: case ARM::t2LDRSBT:
        case ARM::t2LDRDi8: {
          if (Fn.getName().equals("initframe") && MI.getOpcode()==ARM::t2LDRT)
            break; // FIXME: findScratchReg fails. R1 def in IT not handled correctly
          if (Fn.getName().equals("compdecomp") && MI.getOpcode()==ARM::t2LDRT)
            break;
          unsigned Rt = MI.getOperand(0).getReg();
          if (!isRegDangerous(Rt)) break; // FIXME: Corner case? "rand"
          unsigned Tmp = findScratchReg(Fn, MFI, MI, true);
          if (!Tmp) // FIXME
            break;

          MI.getOperand(0).setReg(Tmp);
          MachineInstrBuilder MIB =
            BuildMI(MFI, NMBBI, MI.getDebugLoc(), TII->get(ARM::tMOVr), Rt)
            .addReg(Tmp).add(Pred);

          if (InsideIT)
            MIB.addReg(ARM::ITSTATE,
                       RegState::Implicit | (MI.killsRegister(ARM::ITSTATE)?RegState::Kill:0));
          Changed = true;
          break;
        }

        case ARM::t2STRi12:    case ARM::t2STRi8:
        case ARM::t2STRHi12:   case ARM::t2STRHi8:
        case ARM::t2STRBi12:   case ARM::t2STRBi8:
        case ARM::t2STRT:
        case ARM::t2STRHT:
        case ARM::t2STRBT:
        case ARM::t2STRDi8: {
          unsigned Rt = MI.getOperand(0).getReg();
          if (Rt==ARM::R4) return Modified; // FIXME: Corner case in __ieee754_rem_pio2
          assert(isRegDangerous(Rt));
          unsigned Tmp = findScratchReg(Fn, MFI, MI, true);
          if (!Tmp) // FIXME
            break;

          BuildMI(MFI, MI.getIterator(), MI.getDebugLoc(), TII->get(ARM::tMOVr), Tmp)
            .addReg(Rt).add(Pred);
          MI.getOperand(0).setReg(Tmp);
          assert(!InsideIT);
          Changed = true;
          break;
        }

        case ARM::t2STR_PRE:   case ARM::t2STR_POST:
        case ARM::t2STRH_PRE:  case ARM::t2STRH_POST:
        case ARM::t2STRB_PRE:  case ARM::t2STRB_POST:  {
          unsigned Rt = MI.getOperand(1).getReg();
          assert(isRegDangerous(Rt));
          unsigned Tmp = findScratchReg(Fn, MFI, MI, true);
          BuildMI(MFI, MI.getIterator(), MI.getDebugLoc(), TII->get(ARM::tMOVr), Tmp)
            .addReg(Rt).add(Pred);
          MI.getOperand(1).setReg(Tmp);
          assert(!InsideIT);
          Changed = true;
          break;
        }

        } // end switch

        if (Changed && IT) {
          // assert(ITDist<3);
          // unsigned MIBitPos = 4 - ITDist;
          // unsigned Mask = IT->getOperand(1).getImm();
          // unsigned MaskLow = Mask & ((1 << (MIBitPos+1))-1);
          // unsigned NewMask = (Mask & (-1U << MIBitPos)) // clear lower bits
          //   | (MaskLow>>1) | (0x10 >> std::min(ITCount+1, 4));
          // IT->getOperand(1).setImm(NewMask);

          FixITBlock = true;
          if (ITCount==4) {
            // if (ITDist==3) FixITNow = true;
            dbgs() << "blah\n";
            // FixITBlock = true;

            // check that CPSR is not changed
            // auto TmpMII = IT->getIterator();
            // int TmpCount = 0;
            // while (true) {
            //   ++TmpMII;
            //   if (doesNotGeneratecode(*TmpMII))
            //     continue;
            //   ++TmpCount;
            //   assert(!TmpMII->definesRegister(ARM::CPSR));
            //   if (TmpCount==4)
            //     TmpMII->findRegisterUseOperand(ARM::ITSTATE)->setIsKill();
            //   if (TmpCount==5)
            //     break;
            // }
            // LastIT = &*TmpMII;
          }
        }
      }
    jump:
      if (FixITBlock && KillsITSTATE) {
        // Modify first IT
        unsigned Mask = IT->getOperand(1).getImm() & 0x10;
        unsigned Pos = 3;

        auto TmpMII = std::next(IT->getIterator());
        while (doesNotGeneratecode(*TmpMII)) ++TmpMII;
        unsigned PredReg = 0;
        ARMCC::CondCodes CC = getITInstrPredicate(*TmpMII, PredReg);
        int TmpCount = 1;
        bool NeedNewIT = false;
        while (true) {
          ++TmpMII;
          if (doesNotGeneratecode(*TmpMII))
            continue;
          ++TmpCount;
          // Check that CPSR is never changed inside IT block
          assert(!TmpMII->definesRegister(ARM::CPSR));

          unsigned NPredReg = 0;
          ARMCC::CondCodes NCC = getITInstrPredicate(*TmpMII, NPredReg);
          Mask |= (NCC & 1) << Pos;
          --Pos;

          if (TmpMII->killsRegister(ARM::ITSTATE))
            break;
          if (TmpCount==4) {
            NeedNewIT = true;
            TmpMII->findRegisterUseOperand(ARM::ITSTATE)->setIsKill();
            break;
          }
        }

        Mask |= (1 << Pos);
        IT->getOperand(1).setImm(Mask);

        MachineInstrBuilder MIB;
        if (NeedNewIT) {
          // New IT if needed
          ++TmpMII;
          while (doesNotGeneratecode(*TmpMII)) ++TmpMII;
          assert(!TmpMII->definesRegister(ARM::CPSR));
          TmpCount = 1;
          PredReg = 0;
          CC = getITInstrPredicate(*TmpMII, PredReg);
          MIB = BuildMI(MFI, TmpMII, TmpMII->getDebugLoc(), TII->get(ARM::t2IT)).addImm(CC);
          Mask = 0;
          Pos = 3;
          while (!TmpMII->killsRegister(ARM::ITSTATE)) {
            ++TmpMII;
            if (doesNotGeneratecode(*TmpMII))
              continue;
            ++TmpCount;
            // Check that CPSR is never changed inside IT block
            assert(!TmpMII->definesRegister(ARM::CPSR));

            unsigned NPredReg = 0;
            ARMCC::CondCodes NCC = getITInstrPredicate(*TmpMII, NPredReg);
            Mask |= (NCC & 1) << Pos;
            --Pos;
            assert(TmpCount<=4);
          }
          Mask |= (1 << Pos);
          Mask |= (CC & 1) << 4;
          MIB.addImm(Mask);
        }

        // assert(LastIT->findRegisterUseOperand(ARM::ITSTATE)->isKill());
        // unsigned LastPredReg;
        // ARMCC::CondCodes LastPredCC = getInstrPredicate(*LastIT, LastPredReg);
        // unsigned Mask = 0x8;
        // Mask |= (LastPredCC & 1) << 4;
        // BuildMI(MFI, LastIT->getIterator(), MI.getDebugLoc(), TII->get(ARM::t2IT))
        //   .addImm(LastPredCC).addImm(Mask);
        // FixITBlock = false; FixITNow = false;
      }

      ++CurIndex;
    }
  }

  if (Trial < 1) {
    ++Trial;
    goto restart;
  }

  return Modified;
}

FunctionPass *llvm::createARMRemoveUnintendedPass() {
  return new ARMRemoveUnintended();
}
