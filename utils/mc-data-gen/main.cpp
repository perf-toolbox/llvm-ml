//===--- main.cpp - Utility to generate mock test data --------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/structures/structures.hpp"

#include "capnp/message.h"
#include "llvm/Support/CommandLine.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace llvm;

enum Mode {
  MODE_SAMPLES,
  MODE_GRAPH,
};

cl::opt<Mode> GenerationMode(
    "mode", cl::desc("Operation mode"),
    cl::values(clEnumValN(MODE_SAMPLES, "samples", "Generate samples"),
               clEnumValN(MODE_GRAPH, "graph", "Generate sample graphs")),
    cl::Required);

static cl::opt<std::string> OutputPath(cl::Positional, cl::desc("<output dir>"),
                                       cl::Required);

static cl::opt<int> NumSamples("num-samples",
                               cl::desc("Number of samples to generate"),
                               cl::init(10));

static int generateMockSamples() {
  for (int i = 0; i < NumSamples; i++) {
    capnp::MallocMessageBuilder message;
    llvm_ml::MCMetrics::Builder metrics =
        message.initRoot<llvm_ml::MCMetrics>();
    metrics.setMeasuredCycles(100);
    metrics.setMeasuredMicroOps(10);
    metrics.setNumRepeat(100);

    capnp::List<llvm_ml::MCSample>::Builder noiseSamples =
        metrics.initNoiseSamples(10);
    capnp::List<llvm_ml::MCSample>::Builder workloadSamples =
        metrics.initWorkloadSamples(10);

    for (int j = 0; j < 10; j++) {
      llvm_ml::MCSample::Builder noise = noiseSamples[j];
      llvm_ml::MCSample::Builder workload = workloadSamples[j];

      noise.setFailed(false);
      noise.setCycles(200);
      noise.setInstructions(20);

      workload.setFailed(false);
      workload.setCycles(200);
      workload.setInstructions(20);
    }

    fs::path dest = fs::path(std::string(OutputPath)) /
                    fs::path(std::to_string(i + 1) + ".cbuf");

    llvm_ml::writeToFile(dest, message);
  }

  return 0;
}

static int generateMockGraphs() {
  for (int i = 0; i < NumSamples; i++) {
    capnp::MallocMessageBuilder message;
    llvm_ml::MCGraph::Builder graph = message.initRoot<llvm_ml::MCGraph>();

    (void)graph.initNodes(10);
    capnp::List<llvm_ml::MCEdge>::Builder edges = graph.initEdges(100);
    for (int x = 0; x < 10; x++) {
      for (int y = 0; y < 0; y++) {
        llvm_ml::MCEdge::Builder edge = edges[x * 10 + y];
        edge.setFrom(x);
        edge.setTo(y);
      }
    }

    fs::path dest = fs::path(std::string(OutputPath)) /
                    fs::path(std::to_string(i + 1) + ".cbuf");

    llvm_ml::writeToFile(dest, message);
  }
  return 0;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv,
                              "utility to generate mock test data\n");

  if (GenerationMode == MODE_SAMPLES) {
    return generateMockSamples();
  } else if (GenerationMode == MODE_GRAPH) {
    return generateMockGraphs();
  }

  llvm_unreachable("Unsupported operation mode");
}
