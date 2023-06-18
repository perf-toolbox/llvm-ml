//===--- benchmark.cpp - Benchmark runner interface -------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "benchmark.hpp"

namespace llvm_ml {
std::vector<BenchmarkResult> runBenchmark(BenchmarkFn bench, int pinnedCPU,
                                          int numRuns, int numRep) {
  // Warmup
  for (int i = 0; i < 3; i++) {
    auto res = detail::runSingleBenchmark(bench, pinnedCPU);
    if (!res) {
      // ignore any errors
    }
  }

  std::vector<BenchmarkResult> results;
  results.reserve(numRuns);

  for (int i = 0; i < numRuns; i++) {
    auto res = detail::runSingleBenchmark(bench, pinnedCPU);
    if (res) {
      results.push_back(*res);
      results.back().numRuns = numRep;
    } else {
      results.push_back(BenchmarkResult{.hasFailed = true});
    }
  }

  return results;
}
} // namespace llvm_ml
