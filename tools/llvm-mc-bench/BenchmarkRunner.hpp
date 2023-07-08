//===--- BenchmarkRunner.hpp - Benchmark running tool ---------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Error.h"

#include <memory>

namespace llvm {
class Target;
}

namespace llvm_ml {
class BenchmarkResult;

class BenchmarkRunner {
public:
  virtual llvm::Error run() = 0;
  virtual llvm::ArrayRef<BenchmarkResult> getNoiseResults() const = 0;
  virtual llvm::ArrayRef<BenchmarkResult> getWorkloadResults() const = 0;
};

std::unique_ptr<BenchmarkRunner>
createCPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                         std::unique_ptr<llvm::Module> harness,
                         size_t repeatNoise, size_t repeatWorkload,
                         int numRuns);
} // namespace llvm_ml
