//===--- BenchmarkGenerator.hpp - Benchmark harness generation utils ------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/target/Target.hpp"

#include "llvm/IR/Module.h"

#include <string>

namespace llvm_ml {
inline constexpr float kNoiseFrac = 0.2f;
inline constexpr auto kBaselineNoiseName = "baseline";
inline constexpr auto kWorkloadName = "workload";

inline constexpr size_t kArgCountersCtx = 0;
inline constexpr size_t kArgCountersStart = 1;
inline constexpr size_t kArgCountersStop = 2;
inline constexpr size_t kArgBranchAddr = 3;

/// Signature of a benchmark harness function. The convention is as following:
/// arg0: counters handle
/// arg1: pointer to void counters_start(void*)
/// arg2: pointer to void counters_stop(void*)
/// arg3: pointer to struct __attribute__((packed)) { void *workloadBegin; void
/// *workloadEnd; }
using BenchmarkFn = void (*)(void *, void *, void *, void *);

std::unique_ptr<llvm::Module>
createCPUTestHarness(llvm::LLVMContext &context, std::string basicBlock,
                     int numRepeat, llvm_ml::InlineAsmBuilder &inlineAsm);
} // namespace llvm_ml
