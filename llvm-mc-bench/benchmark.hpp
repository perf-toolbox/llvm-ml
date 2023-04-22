//===--- benchmark.hpp - Benchmark runner interface -------------C++-===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <string>
#include <functional>

namespace llvm {
class Error;
}

namespace llvm_ml {
using BenchmarkFn = void (*)(void *, void *);
using BenchmarkCb = std::function<void(size_t)>;

llvm::Error runBenchmark(BenchmarkFn bench, const BenchmarkCb &cb,
                         int pinnedCPU);
} // namespace llvm_ml

extern "C" {
void benchmark_unmap_before(void *ptr);
void benchmark_unmap_after(void *ptr);
}
