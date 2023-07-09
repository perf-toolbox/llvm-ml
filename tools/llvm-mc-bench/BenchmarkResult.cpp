//===--- BenchmarkResult.hpp - Benchmark result -----------------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkResult.hpp"
#include "lib/structures/mc_metrics.pb.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace llvm_ml {

static void toPBuf(const BenchmarkResult &res, MCSample *sample) {
  sample->set_failed(res.hasFailed);
  sample->set_cycles(res.numCycles);
  sample->set_instructions(res.numInstructions);
  sample->set_uops(res.numMicroOps);
  sample->set_cache_misses(res.numCacheMisses);
  sample->set_context_switches(res.numContextSwitches);
}

void Measurement::exportProtobuf(llvm::raw_ostream &os, llvm::StringRef source,
                                 llvm::ArrayRef<BenchmarkResult> noise,
                                 llvm::ArrayRef<BenchmarkResult> workload) {
  MCMetrics metrics;
  metrics.set_noise_cycles(noiseCycles);
  metrics.set_noise_cache_misses(noiseCacheMisses);
  metrics.set_noise_context_switches(noiseContextSwitches);
  metrics.set_workload_cycles(workloadCycles);
  metrics.set_workload_cache_misses(workloadCacheMisses);
  metrics.set_workload_context_switches(workloadContextSwitches);
  metrics.set_workload_num_runs(workloadNumRuns);
  metrics.set_measured_cycles(measuredCycles);
  metrics.set_measured_num_runs(measuredNumRuns);
  metrics.set_source(source.str());

  for (auto sample : noise) {
    auto *mcSample = metrics.add_noise_samples();
    toPBuf(sample, mcSample);
  }
  for (auto sample : workload) {
    auto *mcSample = metrics.add_noise_samples();
    toPBuf(sample, mcSample);
  }

  std::string serialized;
  metrics.SerializeToString(&serialized);
  os << serialized;
}

static json toJSON(const BenchmarkResult &res) {
  json resJson;
  resJson["failed"] = res.hasFailed;
  resJson["cycles"] = res.numCycles;
  resJson["context_switches"] = res.numContextSwitches;
  resJson["cache_misses"] = res.numCacheMisses;
  resJson["uops"] = res.numMicroOps;
  resJson["instructions"] = res.numInstructions;
  resJson["misaligned_loads"] = res.numMisalignedLoads;

  return resJson;
}

void Measurement::exportJSON(llvm::raw_ostream &os, llvm::StringRef source,
                             llvm::ArrayRef<BenchmarkResult> noise,
                             llvm::ArrayRef<BenchmarkResult> workload) {
  json res;
  res["noise_cycles"] = noiseCycles;
  res["noise_cache_misses"] = noiseCacheMisses;
  res["noise_context_switches"] = noiseContextSwitches;
  res["noise_num_runs"] = noiseNumRuns;
  res["workload_cycles"] = workloadCycles;
  res["workload_cache_misses"] = workloadCacheMisses;
  res["workload_context_switches"] = workloadContextSwitches;
  res["workload_num_runs"] = workloadNumRuns;
  res["measured_cycles"] = measuredCycles;
  res["measured_num_runs"] = measuredNumRuns;
  res["source"] = source;

  auto noiseSamples = json::array();
  auto workloadSamples = json::array();

  for (auto sample : noise) {
    noiseSamples.push_back(toJSON(sample));
  }
  for (auto sample : workload) {
    workloadSamples.push_back(toJSON(sample));
  }

  res["noise_samples"] = noiseSamples;
  res["workload_samples"] = workloadSamples;

  os << res.dump(4);
}

BenchmarkResult avg(llvm::ArrayRef<BenchmarkResult> inputs) {
  const auto minEltPred = [](const auto &lhs, const auto &rhs) {
    return lhs.numCycles < rhs.numCycles;
  };

  llvm::SmallVector<BenchmarkResult> results(inputs);
  std::sort(results.begin(), results.end(), minEltPred);
  results.pop_back_n(2);

  BenchmarkResult avg;

  avg.numRuns = results.front().numRuns;

  size_t numFailed = 0;

  for (auto res : results) {
    if (res.hasFailed) {
      numFailed++;
      continue;
    }
    avg.numCycles += res.numCycles;
    avg.numContextSwitches += res.numContextSwitches;
    avg.numCacheMisses += res.numCacheMisses;
    avg.numMicroOps += res.numMicroOps;
    avg.numInstructions += res.numInstructions;
    avg.numMisalignedLoads += res.numMisalignedLoads;
  }

  const size_t total = results.size() - numFailed;

  avg.numCycles /= total;
  avg.numContextSwitches /= total;
  avg.numCacheMisses /= total;
  avg.numMicroOps /= total;
  avg.numInstructions /= total;
  avg.numMisalignedLoads /= total;

  return avg;
}
} // namespace llvm_ml
