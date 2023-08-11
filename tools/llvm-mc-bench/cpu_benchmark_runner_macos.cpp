//===--- cpu_benchmark_runner_macos.cpp - Benchmark running tool ----------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkGenerator.hpp"
#include "BenchmarkResult.hpp"
#include "BenchmarkRunner.hpp"

#include <memory>

namespace llvm_ml {
std::unique_ptr<BenchmarkRunner>
createCPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                         int pinnedCPU, int numRuns) {
  llvm_unreachable("Not implemented");
}
} // namespace llvm_ml
