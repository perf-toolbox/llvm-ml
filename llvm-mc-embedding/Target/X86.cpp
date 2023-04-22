//===--- X86.cpp - X86-specific utilities ---------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "../Target.hpp"

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"

namespace llvm_ml {
class X86Target : public Target {
public:
  X86Target(llvm::MCRegisterInfo *ri, llvm::MCAsmInfo *ai,
            llvm::MCSubtargetInfo *sti, llvm::MCInstrInfo *ii)
      : mRI(ri), mAI(ai), mSTI(sti), mII(ii) {}

  std::set<unsigned> getReadRegisters(const llvm::MCInst &inst) override {
    std::set<unsigned> readRegs;

    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    for (unsigned opIdx = 0; opIdx < inst.getNumOperands(); opIdx++) {
      const llvm::MCOperand &operand = inst.getOperand(opIdx);
      if (operand.isReg()) {
        if (!desc.operands()[opIdx].isOptionalDef()) {
          readRegs.insert(operand.getReg());
        }
      }
    }

    for (auto &reg : desc.implicit_uses()) {
      readRegs.insert(reg);
    }
    return readRegs;
  }

  std::set<unsigned> getWriteRegisters(const llvm::MCInst &inst) override {
    std::set<unsigned> writeRegs;

    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    for (unsigned opIdx = 0; opIdx < inst.getNumOperands(); opIdx++) {
      const llvm::MCOperand &operand = inst.getOperand(opIdx);
      if (operand.isReg()) {
        if (desc.operands()[opIdx].isOptionalDef()) {
          writeRegs.insert(operand.getReg());
        }
      }
    }

    for (auto &reg : desc.implicit_defs()) {
      writeRegs.insert(reg);
    }

    return writeRegs;
  }

  bool isMemLoad(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    return desc.mayLoad();
  }

  bool isMemStore(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    return desc.mayStore();
  }

  bool isBarrier(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    return desc.isBarrier();
  }

  bool isVector(const llvm::MCInst &inst) override {
    // TODO fix this method
    return false;
  }

  bool isAtomic(const llvm::MCInst &inst) override {
    // TODO fix this method
    return false;
  }

  bool isCompute(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());

    // TODO this is not exactly true, but OK for my current purpose
    if (!desc.mayLoad() && !desc.mayStore() && !desc.isMoveReg() && !desc.isMoveImmediate())
      return true;

    return false;
  }

private:
  llvm::MCRegisterInfo *mRI;
  llvm::MCAsmInfo *mAI;
  llvm::MCSubtargetInfo *mSTI;
  llvm::MCInstrInfo *mII;
};

std::unique_ptr<Target> createX86Target(llvm::MCRegisterInfo *ri,
                                        llvm::MCAsmInfo *ai,
                                        llvm::MCSubtargetInfo *sti,
                                        llvm::MCInstrInfo *ii) {
  return std::make_unique<X86Target>(ri, ai, sti, ii);
}
} // namespace llvm_ml
