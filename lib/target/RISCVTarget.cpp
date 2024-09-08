//===--- RISCVTarget.cpp - X86-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/target/Target.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/FormatVariadic.h"

#include "MCTargetDesc/RISCVBaseInfo.h"

constexpr auto SaveState = R"(
  sd x1, 72(sp)
  sd x2, 80(sp)
  sd x3, 88(sp)
  sd x4, 96(sp)
  sd x5, 104(sp)
  sd x6, 112(sp)
  sd x7, 120(sp)
  sd x8, 128(sp)
  sd x9, 136(sp)
  sd x10, 144(sp)
  sd x11, 152(sp)
  sd x12, 160(sp)
  sd x13, 168(sp)
  sd x14, 176(sp)
  sd x15, 184(sp)
  sd x16, 192(sp)
  sd x17, 200(sp)
  sd x18, 208(sp)
  sd x19, 216(sp)
  sd x20, 224(sp)
  sd x21, 232(sp)
  sd x22, 240(sp)
  sd x23, 248(sp)
  sd x24, 256(sp)
  sd x25, 264(sp)
  sd x26, 272(sp)
  sd x27, 280(sp)
  sd x29, 288(sp)
  sd x30, 296(sp)
  sd x31, 304(sp)
)";

constexpr auto RestoreState = R"(
  ld x1, 72(sp)
  ld x2, 80(sp)
  ld x3, 88(sp)
  ld x4, 96(sp)
  ld x5, 104(sp)
  ld x6, 112(sp)
  ld x7, 120(sp)
  ld x8, 128(sp)
  ld x9, 136(sp)
  ld x10, 144(sp)
  ld x11, 152(sp)
  ld x12, 160(sp)
  ld x13, 168(sp)
  ld x14, 176(sp)
  ld x15, 184(sp)
  ld x16, 192(sp)
  ld x17, 200(sp)
  ld x18, 208(sp)
  ld x19, 216(sp)
  ld x20, 224(sp)
  ld x21, 232(sp)
  ld x22, 240(sp)
  ld x23, 248(sp)
  ld x24, 256(sp)
  ld x25, 264(sp)
  ld x26, 272(sp)
  ld x27, 280(sp)
  ld x29, 288(sp)
  ld x30, 296(sp)
  ld x31, 304(sp)
)";

constexpr auto Prologue = R"(
  li x1, 0x2325000
  sd sp, 0(x1)

  li x1, 0x2324000
  mv x2, x1
  mv x3, x1
  mv x4, x1
  mv x5, x1
  mv x6, x1
  mv x7, x1
  mv x8, x1
  mv x9, x1
  mv x10, x1
  mv x11, x1
  mv x12, x1
  mv x13, x1
  mv x14, x1
  mv x15, x1
  mv x16, x1
  mv x17, x1
  mv x18, x1
  mv x19, x1
  mv x20, x1
  mv x21, x1
  mv x22, x1
  mv x23, x1
  mv x24, x1
  mv x25, x1
  mv x26, x1
  mv x27, x1
  mv x28, x1
  mv x29, x1
  mv x30, x1
  mv x31, x1
)";

constexpr auto Epilogue = R"(
  li x1, 0x2325
  ld sp, 0(x1)
)";

namespace {
class RISCVInlineAsmBuilder : public llvm_ml::InlineAsmBuilder {
public:
  void createSetupEnv(llvm::IRBuilderBase &builder) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);

    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, Prologue,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }

  void createRestoreEnv(llvm::IRBuilderBase &builder) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, Epilogue,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }

  void createSaveState(llvm::IRBuilderBase &builder) override {
    llvm::Type *i32ty = llvm::Type::getInt32Ty(builder.getContext());
    llvm::Type *ptr = i32ty->getPointerTo();
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, SaveState,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }

  void createRestoreState(llvm::IRBuilderBase &builder) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, RestoreState,
                                          "~{dirflag},~{fpsr},~{flags}", true);
    builder.CreateCall(asmCallee);
  }

  void createBranch(llvm::IRBuilderBase &builder,
                    llvm::StringRef label) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto asmCallee =
        llvm::InlineAsm::get(voidFuncTy, ("j " + label).str(),
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

class RISCVTarget : public llvm_ml::MLTarget {
public:
  RISCVTarget(llvm::MCInstrInfo *mcii) : mII(mcii) {}

  std::set<unsigned> getReadRegisters(const llvm::MCInst &inst) override {
    std::set<unsigned> readRegs;

    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    for (unsigned opIdx = 1; opIdx < inst.getNumOperands(); opIdx++) {
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

  bool isImplicitReg(const llvm::MCInst &inst, unsigned reg) override {
    auto pred = [reg](unsigned other) { return other == reg; };

    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());

    return llvm::any_of(desc.implicit_uses(), pred) || llvm::any_of(desc.implicit_defs(), pred);
  }

  bool isVectorReg(unsigned reg) override {
    return false;
  }

  bool isTileReg(unsigned reg) override {
    return false;
  }

  bool isCall(const llvm::MCInst &inst) override {
    const llvm::MCInstrDesc &desc = mII->get(inst.getOpcode());
    return desc.isCall() || inst.getOpcode() == llvm::RISCV::JAL ||
           inst.getOpcode() == llvm::RISCV::JALR;
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
    return false;
  }

  bool isAtomic(const llvm::MCInst &inst) override {
    return false;
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
    if (inst.getOpcode() != llvm::RISCV::ADDI)
      return false;
    return inst.getOperand(0).getReg() == llvm::RISCV::X0;
  }

  bool isFloat(const llvm::MCInst &inst) override {
    return false;
  }

  bool isLea(const llvm::MCInst &inst) override {
    return false;
  }

  bool isPush(const llvm::MCInst &inst) override {
    return false;
  }

  bool isPop(const llvm::MCInst &inst) override {
    return false;
  }

  bool isMov(const llvm::MCInst &inst) override {
    return false;
  }

  bool isSyscall(const llvm::MCInst &inst) override {
    return inst.getOpcode() == llvm::RISCV::ECALL;
  }

  bool isVarLatency(const llvm::MCInst &inst) override {
    return false;
  }

  std::unique_ptr<llvm_ml::InlineAsmBuilder> createInlineAsmBuilder() override {
    return std::make_unique<RISCVInlineAsmBuilder>();
  }

private:
  llvm::MCInstrInfo *mII;
};
} // namespace

namespace llvm_ml {
std::unique_ptr<MLTarget> createRISCVMLTarget(llvm::MCInstrInfo *mcii) {
  return std::make_unique<RISCVTarget>(mcii);
}
} // namespace llvm_ml

