//===--- BenchmarkGenerator.cpp - Benchmark harness generation utils ------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkGenerator.hpp"

#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"

using namespace llvm;
using namespace llvm_ml;

static void createSingleCPUTestFunction(StringRef functionName,
                                        ArrayRef<StringRef> assembly,
                                        int numRepeat, Module &module,
                                        IRBuilderBase &builder,
                                        InlineAsmBuilder &inlineAsm) {
  auto &context = module.getContext();

  auto retTy = Type::getVoidTy(context);
  auto ptrTy = PointerType::getUnqual(context);

  auto funcTy = FunctionType::get(retTy, {ptrTy, ptrTy, ptrTy, ptrTy}, false);

  auto funcCallee = module.getOrInsertFunction(functionName, funcTy);
  (void)funcCallee;

  auto func = module.getFunction(functionName);

  auto *entry = llvm::BasicBlock::Create(context, "entry", func);

  builder.SetInsertPoint(entry);

  auto countersFuncTy = FunctionType::get(retTy, {ptrTy}, false);

  std::string startName = ("workload_start_" + functionName).str();
  std::string endName = ("workload_end_" + functionName).str();

  inlineAsm.createSaveState(builder);
  builder.CreateCall(countersFuncTy, func->getArg(kArgCountersStart),
                     {func->getArg(kArgCountersCtx)});

  inlineAsm.createSetupEnv(builder);

  inlineAsm.createBranch(builder, startName);
  inlineAsm.createLabel(builder, startName);

  auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);

  for (int i = 0; i < numRepeat; i++) {
    for (auto line : assembly) {
      llvm::StringRef trimmed = line.trim();
      if (trimmed.empty())
        continue;
      if (trimmed.startswith(";"))
        continue;
      auto asmCallee = llvm::InlineAsm::get(
          voidFuncTy, trimmed, "~{dirflag},~{fpsr},~{flags}", true);
      builder.CreateCall(asmCallee);
    }
  }

  inlineAsm.createBranch(builder, endName);
  inlineAsm.createLabel(builder, endName);

  inlineAsm.createRestoreEnv(builder);
  inlineAsm.createRestoreState(builder);
  builder.CreateCall(countersFuncTy, func->getArg(kArgCountersStop),
                     {func->getArg(kArgCountersCtx)});

  builder.CreateRetVoid();
}

namespace llvm_ml {
std::unique_ptr<Module>
createCPUTestHarness(LLVMContext &context, std::string microbenchAsm,
                     int numRepeatNoise, int numRepeat,
                     llvm_ml::InlineAsmBuilder &inlineAsm) {
  // Prepare asm string
  {
    size_t pos = 0;

    while (pos = microbenchAsm.find("$", pos), pos != std::string::npos) {
      microbenchAsm.insert(pos, "$");
      pos += 2;
    }
  }

  auto module = std::make_unique<Module>("test_harness", context);
  IRBuilder builder(context);

  StringRef basicBlock = microbenchAsm;
  SmallVector<StringRef> lines;
  basicBlock.split(lines, '\n');

  createSingleCPUTestFunction(kBaselineNoiseName, lines, numRepeatNoise,
                              *module, builder, inlineAsm);

  createSingleCPUTestFunction(kWorkloadName, lines, numRepeat, *module, builder,
                              inlineAsm);

  return module;
}
} // namespace llvm_ml
