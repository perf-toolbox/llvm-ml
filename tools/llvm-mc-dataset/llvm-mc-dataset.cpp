//===--- llvm-mc-dataset.cpp - Dataset preparation tool -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "lib/structures/mc_dataset.pb.h"
#include "lib/structures/mc_graph.pb.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

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

  std::future<std::map<std::string, llvm_ml::MCGraph>> graphFuture =
      std::async(std::launch::async, [&]() {
        std::map<std::string, llvm_ml::MCGraph> graphs;

        for (auto d : fs::directory_iterator(graphsDir)) {
          fs::path path = d.path();
          if (path.extension() != ".pb")
            continue;

          std::fstream ifs{path};
          llvm_ml::MCGraph graph;
          if (!graph.ParseFromIstream(&ifs))
            continue;

          // Double stem in case of two extensions
          graphs[std::string{path.stem().stem().c_str()}] = graph;
        }

        return graphs;
      });

  std::future<std::map<std::string, llvm_ml::MCMetrics>> metricsFuture =
      std::async(std::launch::async, [&]() {
        std::map<std::string, llvm_ml::MCMetrics> metrics;

        for (auto d : fs::directory_iterator(metricsDir)) {
          fs::path path = d.path();
          if (path.extension() != ".pb")
            continue;

          std::fstream ifs{path};
          llvm_ml::MCMetrics metric;
          if (!metric.ParseFromIstream(&ifs))
            continue;

          // Double stem in case of two extensions
          metrics[std::string{path.stem().stem().c_str()}] = metric;
        }

        return metrics;
      });

  graphFuture.wait();
  metricsFuture.wait();

  const auto &graphs = graphFuture.get();
  const auto &metrics = metricsFuture.get();

  llvm_ml::MCDataset dataset;

  for (const auto &m : metrics) {
    if (m.second.measured_cycles() == 0)
      continue;
    if (!graphs.count(m.first))
      continue;

    const auto &graph = graphs.at(m.first);
    if (graph.nodes_size() == 0)
      continue;

    auto *data = dataset.add_data();
    data->mutable_graph()->CopyFrom(graph);
    data->mutable_metrics()->CopyFrom(m.second);
    data->mutable_metrics()->clear_source();
  }

  std::ofstream ofs{OutFile};
  dataset.SerializeToOstream(&ofs);
  ofs.close();

  return 0;
}
