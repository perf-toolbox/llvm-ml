//===--- AArch64Target.cpp - AArch64-specific utils -----------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "Target.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"

#include "MCTargetDesc/AArch64MCTargetDesc.h"

namespace {
class AArch64InlineAsmBuilder : public llvm_ml::InlineAsmBuilder {
public:
  void createSaveState(llvm::IRBuilderBase &builder) override {
    std::string prologue;

    // Save current registers
    for (int i = 0; i < 31; i++) {
      prologue += "push x" + std::to_string(i) + "\n";
    }

    // Save stack pointer
    prologue += "mov x0, sp\n";
    prologue += "mov x1, 36851712\n";
    prologue += "str x0, [x1]\n";

    prologue += "mov sp, 36849664\n";

    // Initialize registers
    for (int i = 0; i < 31; i++) {
      prologue += "mov x" + std::to_string(i) + ", 36847616\n";
    }

    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    // TODO can we use fxsave here?
    // TODO can we just enumerate all registers for current target?
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, prologue,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }

  void createRestoreState(llvm::IRBuilderBase &builder) override {
    std::string epilogue;

    epilogue += "mov x1, 36851712\n";
    epilogue += "ldr x0, [x1]\n";
    epilogue += "mov sp, x0\n";

    for (int i = 30; i >= 0; i--) {
      epilogue += "pop x" + std::to_string(i) + "\n";
    }

    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, epilogue,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }
  void createBranch(llvm::IRBuilderBase &builder,
                    llvm::StringRef label) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee =
        llvm::InlineAsm::get(voidFuncTy, ("br " + label).str(),
                             "~{dirflag},~{fpsr},~{flags}", false, true);
    builder.CreateCall(asmCallee);
  }
  void createLabel(llvm::IRBuilderBase &builder,
                   llvm::StringRef labelName) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, (labelName + ":").str(),
                                          "", false, true);
    builder.CreateCall(asmCallee);
  }
};
class AArch64Target : public llvm_ml::MLTarget {
public:
  AArch64Target(llvm::MCInstrInfo *mcii) : mII(mcii) {}

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
    llvm_unreachable("Not implemented");
  }

  bool isAtomic(const llvm::MCInst &inst) override {
    llvm_unreachable("Not implemented");
  }

  bool isCompute(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());

    // TODO this is not exactly true, but OK for my current purpose
    if (!desc.mayLoad() && !desc.mayStore() && !desc.isMoveReg() &&
        !desc.isMoveImmediate())
      return true;

    return false;
  }

  bool isNop(const llvm::MCInst &inst) override {
    // TODO this is not exactly true, but OK for my current purpose
    return false;
  }

  bool isFloat(const llvm::MCInst &inst) override {
    llvm_unreachable("Not implemented");
  }

  bool isLea(const llvm::MCInst &inst) override {
    unsigned opcode = inst.getOpcode();
    return opcode == llvm::AArch64::ADRP || opcode == llvm::AArch64::ADR;
  }

  bool isPush(const llvm::MCInst &inst) override {
    unsigned opcode = inst.getOpcode();
    return opcode == llvm::AArch64::GCSPUSHM ||
           opcode == llvm::AArch64::GCSPUSHX;
  }

  bool isPop(const llvm::MCInst &inst) override {
    unsigned opcode = inst.getOpcode();
    return opcode == llvm::AArch64::GCSPOPCX ||
           opcode == llvm::AArch64::GCSPOPM || opcode == llvm::AArch64::GCSPOPX;
  }

  bool isMov(const llvm::MCInst &inst) override {
    unsigned opcode = inst.getOpcode();
    return opcode >= llvm::AArch64::MOVAZ_2ZMI_H_B &&
           opcode <= llvm::AArch64::MOVZXi;
  }

  std::unique_ptr<llvm_ml::InlineAsmBuilder> createInlineAsmBuilder() override {
    return std::make_unique<AArch64InlineAsmBuilder>();
  }

private:
  llvm::MCInstrInfo *mII;
};
} // namespace

namespace llvm_ml {
std::unique_ptr<MLTarget> createAArch64MLTarget(llvm::MCInstrInfo *mcii) {
  return std::make_unique<AArch64Target>(mcii);
}
} // namespace llvm_ml
