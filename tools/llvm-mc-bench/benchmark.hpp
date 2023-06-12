//===--- benchmark.hpp - Benchmark runner interface -------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include "BenchmarkResult.hpp"

#include "llvm/Support/Error.h"

#include <functional>
#include <string>
#include <vector>

namespace llvm_ml {
using BenchmarkFn = void (*)(void *, void *);

std::vector<BenchmarkResult> runBenchmark(BenchmarkFn bench, int pinnedCPU,
                                          int numRuns);

namespace detail {
llvm::Expected<BenchmarkResult> runSingleBenchmark(BenchmarkFn bench,
                                                   int pinnedCPU);
}
} // namespace llvm_ml
