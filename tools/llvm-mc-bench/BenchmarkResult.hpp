//===--- BenchmarkResult.hpp - Benchmark result -----------------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>

namespace llvm_ml {
struct BenchmarkResult;

/// Measurement represents the final result of the benchmark: workload minus
/// system noise.
struct Measurement {
  uint64_t measuredCycles;
  uint64_t measuredNumRuns;
  uint64_t workloadCycles;
  uint64_t workloadContextSwitches;
  uint64_t workloadCacheMisses;
  uint64_t workloadMicroOps;
  uint64_t workloadInstructions;
  uint64_t workloadNumRuns;
  uint64_t noiseCycles;
  uint64_t noiseContextSwitches;
  uint64_t noiseCacheMisses;
  uint64_t noiseMicroOps;
  uint64_t noiseInstructions;
  uint64_t noiseNumRuns;

  void exportProtobuf(llvm::raw_ostream &os, llvm::StringRef source,
                      llvm::ArrayRef<BenchmarkResult> noise = {},
                      llvm::ArrayRef<BenchmarkResult> workload = {});
  void exportJSON(llvm::raw_ostream &os, llvm::StringRef source,
                  llvm::ArrayRef<BenchmarkResult> noise = {},
                  llvm::ArrayRef<BenchmarkResult> workload = {});
};

/// BenchmarkResult is a result of a single harrness run, whether it is a noise
/// or workload run.
struct BenchmarkResult {
  bool hasFailed = false; ///< true when the benchmark has failed for any reason
  uint64_t numCycles =
      0; ///< number of hardware cycles for target basic block, may be noisy
  uint64_t numContextSwitches =
      0; ///< number of context switches occured during harrness execution
  uint64_t numCacheMisses = 0; ///< number of L1 data cache misses
  uint64_t numMicroOps =
      0; ///< number of micro-operations retired, may be 0 on some platforms
  uint64_t numInstructions = 0; ///< number of HW instructions retired
  uint64_t numMisalignedLoads = 0;
};

inline Measurement operator-(const BenchmarkResult &wl,
                             const BenchmarkResult &noise) {
  Measurement m;

  if (wl.numCycles > noise.numCycles) {
    m.measuredCycles = wl.numCycles - noise.numCycles;
  } else {
    m.measuredCycles = 0;
  }

  m.workloadCycles = wl.numCycles;
  m.workloadContextSwitches = wl.numContextSwitches;
  m.workloadCacheMisses = wl.numCacheMisses;
  m.workloadMicroOps = wl.numMicroOps;
  m.workloadInstructions = wl.numMicroOps;
  m.noiseCycles = noise.numCycles;
  m.noiseContextSwitches = noise.numContextSwitches;
  m.noiseCacheMisses = noise.numCacheMisses;
  m.noiseMicroOps = noise.numMicroOps;
  m.noiseInstructions = noise.numInstructions;

  return m;
}
} // namespace llvm_ml
