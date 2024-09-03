//===--- RISCVTarget.cpp - X86-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "Target.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
// #include "llvm/IR/IntrinsicsX86.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/FormatVariadic.h"

#include "MCTargetDesc/RISCVBaseInfo.h"

constexpr auto SaveState = R"(
  push %rax
  push %rbx
  push %rcx
  push %rdx
  push %rsi
  push %rdi
  push %r8
  push %r9
  push %r10
  push %r11
  push %r12
  push %r13
  push %r14
  push %r15

  movq $$0xFFFFFFFFFFFFFFFF, %rax
  movq $$0xFFFFFFFFFFFFFFFF, %rdx
  xsave64 0x2323000

  vzeroall

  add $$-128, %rsp
  pushf
  orl $$0x40000, (%rsp)
  popf
  sub $$-128, %rsp
)";

constexpr auto RestoreState = R"(
  movq $$0xFFFFFFFFFFFFFFFF, %rax
  movq $$0xFFFFFFFFFFFFFFFF, %rdx
  xrstor64 0x2323000

  pop %r15
  pop %r14
  pop %r13
  pop %r12
  pop %r11
  pop %r10
  pop %r9
  pop %r8
  pop %rdi
  pop %rsi
  pop %rdx
  pop %rcx
  pop %rbx
  pop %rax
)";

constexpr auto PrologueX64 = R"(
  movq %rbp, %rax
  movq $$0x2325000, %rbx
  movq %rax, (%rbx)

  movq %rsp, %rax
  movq %rax, 16(%rbx)

  movq $$512, %rdi
  movq $$0x2324000, %rbx
  shr $$12, %rbx
  shl $$12, %rbx

  movq %rax, %rbp
  add $$2048, %rbp
  mov %rbp, %rsp
  shr $$5, %rsp
  shl $$5, %rsp
  sub $$0x10, %rsp

  movq $$0x2324000, %rax 
  movq $$0x2324000, %rbx  
  movq $$0x2324000, %rcx 
  movq $$0x2324000, %rdx 
  movq $$0x2324000, %rsi 
  movq $$0x2324000, %rdi 
  movq $$0x2324000, %r8  
  movq $$0x2324000, %r9  
  movq $$0x2324000, %r10 
  movq $$0x2324000, %r11 
  movq $$0x2324000, %r12 
  movq $$0x2324000, %r13 
  movq $$0x2324000, %r14 
  movq $$0x2324000, %r15 
)";

constexpr auto Epilogue = R"(
  add $$-128, %rsp
  pushf
  andl $$0xFFFFFFFFFFFBFFFF, (%rsp)
  popf
  sub $$-128, %rsp

  movq $$0x2325000, %rbx
  movq (%rbx), %rax
  movq %rax, %rbp

  movq 16(%rbx), %rax
  movq %rax, %rsp
)";

namespace {
class RISCVInlineAsmBuilder : public llvm_ml::InlineAsmBuilder {
public:
  void createSetupEnv(llvm::IRBuilderBase &builder) override {
    auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);

    auto asmCallee = llvm::InlineAsm::get(voidFuncTy, PrologueX64,
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
    auto alloca = builder.CreateAlloca(ptr);
    // constexpr unsigned defaultValue = 0x1f80;
    // constexpr unsigned flushToZero = 0x8000;
    // constexpr unsigned underflowMask = 0x0800;
    // constexpr unsigned overflowMask = 0x0400;
    // constexpr unsigned denormalsAreZeros = 0x0040;
    // constexpr unsigned divideByZeroMask = 0x0200;
    // auto val = llvm::ConstantInt::get(
    //     i32ty, defaultValue & flushToZero & !underflowMask & !overflowMask &
    //                denormalsAreZeros & !divideByZeroMask);
    // builder.CreateStore(val, alloca);
    //
    // builder.CreateIntrinsic(builder.getVoidTy(),
    //                         llvm::Intrinsic::x86_sse_ldmxcsr, {alloca});
    //
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

    // constexpr unsigned defaultValue = 0x1f80;
    // llvm::Type *i32ty = llvm::Type::getInt32Ty(builder.getContext());
    // llvm::Type *ptr = i32ty->getPointerTo();
    // auto alloca = builder.CreateAlloca(ptr);
    // auto val = llvm::ConstantInt::get(i32ty, defaultValue);
    // builder.CreateStore(val, alloca);

    // builder.CreateIntrinsic(builder.getVoidTy(),
    //                         llvm::Intrinsic::x86_sse_ldmxcsr, {alloca});
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

