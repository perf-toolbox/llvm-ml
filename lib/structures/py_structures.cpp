#include "llvm-ml/structures/structures.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nb = nanobind;
namespace fs = std::filesystem;
using namespace nb::literals;

struct PyMetrics {
  bool failed;
  std::uint64_t cycles;
  std::uint64_t instructions;
  std::uint64_t microOps;
  std::uint16_t cacheMisses;
  std::uint16_t contextSwitches;
  std::uint16_t numRepeat;
};

struct PyNode {
  std::uint16_t nodeId;
  std::uint32_t opcode;
  std::array<bool, 32> binaryOpcode;
  bool isLoad;
  bool isStore;
  bool isBarrier;
  bool isAtomic;
  bool isVector;
  bool isCompute;
  bool isFloat;
  bool isVirtualRoot;
};

struct PyEdge {
  std::uint16_t from;
  std::uint16_t to;
  bool isDataDependency;
};

struct PyBasicBlock {
  std::vector<PyEdge> edges;
  std::vector<PyNode> nodes;
  std::vector<PyMetrics> noiseMetrics;
  std::vector<PyMetrics> workloadMetrics;
  float measuredCycles;
  std::uint16_t numRepeat;
  std::string source;
  bool hasVirtualRoot;
};

std::vector<PyBasicBlock> loadDataset(const std::string &path, bool undirected,
                                      bool includeMetrics) {
  std::vector<PyBasicBlock> result;

  const auto reader = [&result, includeMetrics,
                       undirected](llvm_ml::MCDataset::Reader &dataset) {
    result.reserve(dataset.getData().size());

    for (auto piece : dataset.getData()) {
      PyBasicBlock bb;
      llvm_ml::MCMetrics::Reader metrics = piece.getMetrics();
      llvm_ml::MCGraph::Reader graph = piece.getGraph();
      bb.measuredCycles =
          (float)metrics.getMeasuredCycles() / metrics.getNumRepeat();
      bb.numRepeat = metrics.getNumRepeat();
      bb.source = graph.getSource();
      bb.hasVirtualRoot = graph.getHasVirtualRoot();

      bb.nodes.reserve(graph.getNodes().size());
      bb.edges.reserve(graph.getEdges().size());

      for (const auto &n : graph.getNodes()) {
        PyNode node;

        node.nodeId = n.getNodeId();
        node.opcode = n.getOpcode();

        for (unsigned char i = 0; i < 32; i++) {
          node.binaryOpcode[i] = node.opcode & (1 << i);
        }

        node.isLoad = n.getIsLoad();
        node.isStore = n.getIsStore();
        node.isBarrier = n.getIsBarrier();
        node.isAtomic = n.getIsAtomic();
        node.isVector = n.getIsVector();
        node.isCompute = n.getIsCompute();
        node.isFloat = n.getIsFloat();
        node.isVirtualRoot = n.getIsVirtualRoot();

        bb.nodes.push_back(node);
      }

      for (const auto &e : graph.getEdges()) {
        PyEdge edge;

        edge.from = e.getFrom();
        edge.to = e.getTo();
        edge.isDataDependency = e.getIsDataDependency();

        bb.edges.push_back(edge);

        if (undirected) {
          PyEdge edge;

          edge.from = e.getTo();
          edge.to = e.getFrom();
          edge.isDataDependency = false;
          bb.edges.push_back(edge);
        }
      }

      if (includeMetrics) {
        const auto addMetrics = [](const auto &src, auto &dst) {
          for (const auto &m : src) {
            PyMetrics metrics;

            metrics.failed = m.getFailed();
            metrics.cycles = m.getCycles();
            metrics.instructions = m.getInstructions();
            metrics.microOps = m.getMicroOps();
            metrics.cacheMisses = m.getCacheMisses();
            metrics.contextSwitches = m.getContextSwitches();
            metrics.numRepeat = m.getNumRepeat();

            dst.push_back(metrics);
          }
        };

        addMetrics(metrics.getNoiseSamples(), bb.noiseMetrics);
        addMetrics(metrics.getWorkloadSamples(), bb.workloadMetrics);
      }

      result.push_back(bb);
    }
  };
  int err = llvm_ml::readFromFile<llvm_ml::MCDataset>(fs::path(path), reader);
  if (err != 0)
    abort();

  return result;
}

NB_MODULE(_llvm_ml_impl, m) {
  nb::class_<PyNode>(m, "MCNode")
      .def(nb::init<>())
      .def_rw("node_id", &PyNode::nodeId)
      .def_rw("opcode", &PyNode::opcode)
      .def_rw("binary_opcode", &PyNode::binaryOpcode)
      .def_rw("is_load", &PyNode::isLoad)
      .def_rw("is_store", &PyNode::isStore)
      .def_rw("is_barrier", &PyNode::isBarrier)
      .def_rw("is_atomic", &PyNode::isAtomic)
      .def_rw("is_vector", &PyNode::isVector)
      .def_rw("is_compute", &PyNode::isCompute)
      .def_rw("is_float", &PyNode::isFloat)
      .def_rw("is_virtual_root", &PyNode::isVirtualRoot);
  nb::class_<PyEdge>(m, "MCEdge")
      .def(nb::init<>())
      .def_rw("from_node", &PyEdge::from)
      .def_rw("to_node", &PyEdge::to)
      .def_rw("is_data", &PyEdge::isDataDependency);
  nb::class_<PyMetrics>(m, "MCMetrics")
      .def(nb::init<>())
      .def_rw("failed", &PyMetrics::failed)
      .def_rw("cycles", &PyMetrics::cycles)
      .def_rw("instructions", &PyMetrics::instructions)
      .def_rw("uops", &PyMetrics::microOps)
      .def_rw("cache_misses", &PyMetrics::cacheMisses)
      .def_rw("context_switches", &PyMetrics::contextSwitches)
      .def_rw("num_repeat", &PyMetrics::numRepeat);
  nb::class_<PyBasicBlock>(m, "BasicBlock")
      .def(nb::init<>())
      .def_rw("edges", &PyBasicBlock::edges)
      .def_rw("nodes", &PyBasicBlock::nodes)
      .def_rw("noise_metrics", &PyBasicBlock::noiseMetrics)
      .def_rw("workload_metrics", &PyBasicBlock::workloadMetrics)
      .def_rw("measured_cycles", &PyBasicBlock::measuredCycles)
      .def_rw("num_repeat", &PyBasicBlock::numRepeat)
      .def_rw("has_virtual_root", &PyBasicBlock::hasVirtualRoot)
      .def_rw("source", &PyBasicBlock::source);

  m.def("load_dataset", &loadDataset, "path"_a, "undirected"_a = false,
        "include_metrics"_a = true);
}
