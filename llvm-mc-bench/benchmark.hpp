//===--- benchmark.hpp - Benchmark runner interface -------------C++-===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <string>

namespace llvm {
class Error;
}

namespace llvm_ml {
using BenchmarkFn = size_t (*)(void *);

llvm::Error run_benchmark(BenchmarkFn bench, const std::string &outFile);
} // namespace llvm_ml

extern "C" {
void benchmark_unmap_before(void *ptr);
void benchmark_unmap_after(void *ptr);
}
