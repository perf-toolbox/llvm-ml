//===--- llvm-mc-dataset.cpp - Dataset preparation tool -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/structures/structures.hpp"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

#include <capnp/message.h>
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

  std::vector<std::pair<kj::Own<llvm_ml::MCGraph::Reader>,
                        kj::Own<llvm_ml::MCMetrics::Reader>>>
      measuredPairs;

  {
    const auto &graphs = graphFuture.get();
    const auto &metrics = metricsFuture.get();

    measuredPairs.reserve(metrics.size());

    for (auto &m : metrics) {
      if (m.second->getMeasuredCycles() == 0)
        continue;
      if (!graphs.count(m.first))
        continue;

      measuredPairs.push_back(std::make_pair(capnp::clone(*graphs.at(m.first)),
                                             capnp::clone(*m.second)));
    }
  }

  capnp::List<llvm_ml::MCDataPiece>::Builder pieces =
      dataset.initData(measuredPairs.size());
  for (size_t i = 0; i < measuredPairs.size(); i++) {
    llvm_ml::MCDataPiece::Builder piece = pieces[i];
    piece.setGraph(*measuredPairs[i].first);
    piece.setMetrics(*measuredPairs[i].second);
  }

  llvm_ml::writeToFile(std::string(OutFile), message);

  return 0;
}
