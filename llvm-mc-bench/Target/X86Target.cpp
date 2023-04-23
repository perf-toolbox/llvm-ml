//===--- X86Target.cpp - X86-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "../Target.hpp"

// TODO(Alex) I wonder if the same can be achieved by simply putting
// all of the assembly basic blocks into their own function. Would this
// make the code more portable and concise?
constexpr auto PrologueX64 = R"(
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

constexpr auto PrologueAVX = R"(
  pushq %rax
  vbroadcastsd 40(%rsp), %ymm0
  vbroadcastsd 40(%rsp), %ymm1
  vbroadcastsd 40(%rsp), %ymm2
  vbroadcastsd 40(%rsp), %ymm3
  vbroadcastsd 40(%rsp), %ymm4
  vbroadcastsd 40(%rsp), %ymm5
  vbroadcastsd 40(%rsp), %ymm6
  vbroadcastsd 40(%rsp), %ymm7
  vbroadcastsd 40(%rsp), %ymm8
  vbroadcastsd 40(%rsp), %ymm9
  vbroadcastsd 40(%rsp), %ymm10
  vbroadcastsd 40(%rsp), %ymm11
  vbroadcastsd 40(%rsp), %ymm12
  vbroadcastsd 40(%rsp), %ymm13
  vbroadcastsd 40(%rsp), %ymm14
  vbroadcastsd 40(%rsp), %ymm15
  popq %rax
)";

constexpr auto Epilogue = R"(
  movq $$0x2325000, %rbx
  movq (%rbx), %rax
  movq %rax, %rbp

  movq 16(%rbx), %rax
  movq %rax, %rsp

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

namespace {
class X86Target : public llvm_ml::Target {
public:
  std::string getPrologue() override {
    return std::string(PrologueX64) + std::string(PrologueAVX);
  }
  std::string getEpilogue() override { return std::string(Epilogue); }
};
} // namespace

namespace llvm_ml {
std::unique_ptr<Target> createX86Target() {
  return std::make_unique<X86Target>();
}
} // namespace llvm_ml
