//===--- BenchmarkResult.hpp - Benchmark result -----------------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkResult.hpp"
#include "llvm-ml/structures/structures.hpp"

#include <capnp/message.h>
#include <nlohmann/json.hpp>

#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace llvm_ml {

static void toCapNProto(const BenchmarkResult &res, MCSample::Builder sample) {
  sample.setFailed(res.hasFailed);
  sample.setCycles(res.numCycles);
  sample.setInstructions(res.numInstructions);
  sample.setMicroOps(res.numMicroOps);
  sample.setCacheMisses(res.numCacheMisses);
  sample.setContextSwitches(res.numContextSwitches);
  sample.setNumRepeat(res.numRuns);
}

void Measurement::exportBinary(fs::path path, llvm::StringRef source,
                               llvm::ArrayRef<BenchmarkResult> noise,
                               llvm::ArrayRef<BenchmarkResult> workload) {
  capnp::MallocMessageBuilder message;
  MCMetrics::Builder metrics = message.initRoot<llvm_ml::MCMetrics>();
  metrics.setMeasuredCycles(measuredCycles);
  metrics.setNumRepeat(measuredNumRuns);
  metrics.setSource(source.str());

  capnp::List<llvm_ml::MCSample>::Builder noiseSamples =
      metrics.initNoiseSamples(noise.size());
  capnp::List<llvm_ml::MCSample>::Builder workloadSamples =
      metrics.initNoiseSamples(workload.size());

  const auto converter = [&](capnp::List<llvm_ml::MCSample>::Builder &out) {
    return
        [&](auto result) { toCapNProto(result.value(), out[result.index()]); };
  };

  llvm::for_each(llvm::enumerate(noise), converter(noiseSamples));
  llvm::for_each(llvm::enumerate(workload), converter(workloadSamples));

  llvm_ml::writeToFile(path, message);
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
  resJson["num_repeat"] = res.numRuns;
  resJson["wall_time_ns"] = res.wallTime;

  return resJson;
}

void Measurement::exportJSON(std::filesystem::path path, llvm::StringRef source,
                             llvm::ArrayRef<BenchmarkResult> noise,
                             llvm::ArrayRef<BenchmarkResult> workload) {
  std::error_code ec;
  llvm::raw_fd_ostream os(path.c_str(), ec);
  // TODO check ec and return an error

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
