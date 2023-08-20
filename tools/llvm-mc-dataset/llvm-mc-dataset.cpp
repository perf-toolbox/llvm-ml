//===--- llvm-mc-dataset.cpp - Dataset preparation tool -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/structures/structures.hpp"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

#include <capnp/message.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>

using namespace llvm;
namespace fs = std::filesystem;

static cl::opt<std::string> GraphDirectory(cl::Positional,
                                           cl::desc("<graph dir>"));

static cl::opt<std::string> MetricsDirectory(cl::Positional,
                                             cl::desc("<metrics dir>"));

static cl::opt<std::string> OutFile("o", cl::desc("out file"), cl::Required);

static cl::opt<unsigned>
    MaxCoV("max-cov",
           cl::desc("maximum allowed coefficient of variation, integer value "
                    "in the range 1 to 100"),
           cl::init(10));

static double average(const llvm_ml::MCMetrics::Reader &reader) {
  double sum = 0.0;
  for (const auto &sample : reader.getWorkloadSamples()) {
    sum += sample.getCycles();
  }

  return sum / reader.getWorkloadSamples().size();
}

static double standardDeviation(const llvm_ml::MCMetrics::Reader &reader,
                                double mean) {
  const auto square_diff = [](uint64_t a, double b) -> double {
    double diff = static_cast<double>(a) - b;
    return diff * diff;
  };

  double sum = 0.0;

  for (const auto &sample : reader.getWorkloadSamples()) {
    sum += square_diff(sample.getCycles(), mean);
  }

  return std::sqrt(sum / reader.getWorkloadSamples().size());
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "form Machine Code dataset\n");

  fs::path graphsDir{GraphDirectory.c_str()};
  fs::path metricsDir{MetricsDirectory.c_str()};

  if (!fs::exists(graphsDir)) {
    llvm::errs() << "Graphs directory does not exist\n";
    return 1;
  }
  if (!fs::exists(metricsDir)) {
    llvm::errs() << "Metrics directory does not exist\n";
    return 1;
  }
  if (!fs::is_directory(graphsDir)) {
    llvm::errs() << "Graphs path is not a directory\n";
    return 1;
  }
  if (!fs::is_directory(metricsDir)) {
    llvm::errs() << "Metrics path is not a directory\n";
    return 1;
  }

  if (MaxCoV < 1 || MaxCoV > 100) {
    llvm::errs() << "maximum CoV must be in range [1; 100], got " << MaxCoV
                 << "\n";
    return 1;
  }

  std::future<std::map<std::string, kj::Own<llvm_ml::MCGraph::Reader>>>
      graphFuture = std::async(std::launch::async, [&]() {
        std::map<std::string, kj::Own<llvm_ml::MCGraph::Reader>> graphs;

        for (auto d : fs::directory_iterator(graphsDir)) {
          fs::path path = d.path();
          if (path.extension() != ".cbuf")
            continue;

          const auto appendGraph = [&graphs,
                                    &path](llvm_ml::MCGraph::Reader &graph) {
            // Double stem in case of two extensions
            graphs[std::string{path.stem().stem().c_str()}] =
                capnp::clone(graph);
          };

          if (!llvm_ml::readFromFile<llvm_ml::MCGraph>(path, appendGraph))
            continue;
        }

        return graphs;
      });

  std::future<std::map<std::string, kj::Own<llvm_ml::MCMetrics::Reader>>>
      metricsFuture = std::async(std::launch::async, [&]() {
        std::map<std::string, kj::Own<llvm_ml::MCMetrics::Reader>> metrics;

        for (auto d : fs::directory_iterator(metricsDir)) {
          fs::path path = d.path();
          if (path.extension() != ".cbuf")
            continue;

          const auto appendMetrics =
              [&metrics, &path](llvm_ml::MCMetrics::Reader &metric) {
                // Double stem in case of two extensions
                metrics[std::string{path.stem().stem().c_str()}] =
                    capnp::clone(metric);
              };
          if (!llvm_ml::readFromFile<llvm_ml::MCMetrics>(path, appendMetrics))
            continue;
        }

        return metrics;
      });

  graphFuture.wait();
  metricsFuture.wait();

  capnp::MallocMessageBuilder message;
  llvm_ml::MCDataset::Builder dataset = message.initRoot<llvm_ml::MCDataset>();

  std::vector<std::tuple<std::string, kj::Own<llvm_ml::MCGraph::Reader>,
                         kj::Own<llvm_ml::MCMetrics::Reader>>>
      measuredPairs;

  {
    const auto &graphs = graphFuture.get();
    const auto &metrics = metricsFuture.get();

    measuredPairs.reserve(metrics.size());

    const double maxCoV = static_cast<double>(MaxCoV) / 100.0;

    for (auto &m : metrics) {
      if (m.second->getMeasuredCycles() == 0)
        continue;
      if (!graphs.count(m.first))
        continue;

      if (MaxCoV != 100) {
        double mean = average(*m.second);
        double sigma = standardDeviation(*m.second, mean);

        double cov = mean / sigma;

        if (cov > maxCoV)
          continue;
      }

      measuredPairs.push_back(std::make_tuple(
          m.first, capnp::clone(*graphs.at(m.first)), capnp::clone(*m.second)));
    }
  }

  capnp::List<llvm_ml::MCDataPiece>::Builder pieces =
      dataset.initData(measuredPairs.size());
  for (size_t i = 0; i < measuredPairs.size(); i++) {
    llvm_ml::MCDataPiece::Builder piece = pieces[i];
    piece.setGraph(*std::get<1>(measuredPairs[i]));
    piece.setMetrics(*std::get<2>(measuredPairs[i]));
    piece.setId(std::get<0>(measuredPairs[i]));
  }

  llvm_ml::writeToFile(std::string(OutFile), message);

  return 0;
}
