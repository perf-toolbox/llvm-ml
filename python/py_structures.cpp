#include "llvm-ml/structures/structures.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nb = nanobind;
namespace fs = std::filesystem;
using namespace nb::literals;

template <typename Target> struct PyBasicBlock {
  bool hasVirtualRoot;
  float measuredCycles;
  float cov; ///< Coefficient of variation
  std::string source;
  std::string id;
  nb::ndarray<Target, int> nodes;
  nb::ndarray<Target, long> edges;
  nb::ndarray<Target, float> features;
};

template <typename Target>
std::vector<PyBasicBlock<Target>> loadDataset(const std::string &path,
                                              int startOpcode, int padOpcode) {
  std::vector<PyBasicBlock<Target>> result;

  const auto reader = [&result, startOpcode, padOpcode](llvm_ml::MCDataset::Reader &dataset) {
    result.reserve(dataset.getData().size());

    for (auto piece : dataset.getData()) {
      PyBasicBlock<Target> bb;
      llvm_ml::MCMetrics::Reader metrics = piece.getMetrics();
      llvm_ml::MCGraph::Reader graph = piece.getGraph();
      bb.measuredCycles =
          (float)metrics.getMeasuredCycles() / metrics.getNumRepeat();

      bb.source = graph.getSource();
      bb.id = piece.getId();
      bb.cov = piece.getCov();
      bb.hasVirtualRoot = graph.getHasVirtualRoot();

      struct Container {
        std::vector<int> nodes;
        std::vector<long> edges;
        std::vector<float> features;
      };

      constexpr size_t numFeatures = 4;

      Container *container = new Container();
      nb::capsule owner(container,
                        [](void *p) noexcept { delete (Container *)p; });

      constexpr size_t edgeScale = 2;

      constexpr size_t padEdges = 2;

      // Pre-allocate more space than necessary to avoid reallocations
      container->nodes.reserve(graph.getNodes().size() + 1);
      container->edges.reserve((graph.getEdges().size() + padEdges) *
                               edgeScale + graph.getNodes().size() * 2);
      container->features.reserve(container->edges.size() * numFeatures);

      std::transform(graph.getNodes().begin(), graph.getNodes().end(),
                     std::back_inserter(container->nodes),
                     [](const auto &node) { return node.getOpcode(); });

      for (const auto &edge : graph.getEdges()) {
        if (edge.getFrom() >= container->nodes.size() || edge.getTo() >= container->nodes.size())
          continue;
        if (edge.getFrom() >= 100 || edge.getTo() >= 100)
          continue;
        container->edges.push_back(edge.getFrom());
        container->edges.push_back(edge.getTo());

        container->features.push_back(static_cast<float>(edge.getIsDataDependency()));
        container->features.push_back(static_cast<float>(edge.getIsImplicit()));
        container->features.push_back(static_cast<float>(edge.getIsVector()));
        container->features.push_back(static_cast<float>(!edge.getIsDataDependency()));
      }

      const auto addEmptyFeature = [&] {
        container->features.push_back(0);
        container->features.push_back(0);
        container->features.push_back(0);
        container->features.push_back(1.f);
      };

      container->nodes[0] = startOpcode;
      container->nodes.push_back(padOpcode);
      // Put an edge between the virtual root and the end of sequence
      container->edges.push_back(0);
      container->edges.push_back(container->nodes.size() - 1);
      // Put an edge between the last node and the end of sequence
      container->edges.push_back(container->nodes.size() - 2);
      container->edges.push_back(container->nodes.size() - 1);

      // One feature per edge
      addEmptyFeature();
      addEmptyFeature();

      const size_t numEdges = container->edges.size() / 2;

      std::array<size_t, 1> nodesShape{container->nodes.size()};
      std::array<size_t, 2> edgesShape{numEdges, 2};
      std::array<size_t, 2> featuresShape{numEdges, numFeatures};
      bb.nodes = nb::ndarray<Target, int>(container->nodes.data(), 1,
                                          nodesShape.data(), owner);
      bb.edges = nb::ndarray<Target, long>(container->edges.data(), 2,
                                          edgesShape.data(), owner);
      bb.features = nb::ndarray<Target, float>(container->features.data(), 2,
                                             featuresShape.data(), owner);

      result.push_back(std::move(bb));
    }
  };

  if (llvm_ml::readFromFile<llvm_ml::MCDataset>(fs::path(path), reader) != 0)
    abort();

  return result;
}

NB_MODULE(_llvm_ml_impl, m) {
  nb::class_<PyBasicBlock<nb::pytorch>>(m, "TorchBasicBlock")
      .def(nb::init<>())
      .def_rw("edges", &PyBasicBlock<nb::pytorch>::edges)
      .def_rw("nodes", &PyBasicBlock<nb::pytorch>::nodes)
      .def_rw("features", &PyBasicBlock<nb::pytorch>::features)
      .def_rw("measured_cycles", &PyBasicBlock<nb::pytorch>::measuredCycles)
      .def_rw("has_virtual_root", &PyBasicBlock<nb::pytorch>::hasVirtualRoot)
      .def_rw("id", &PyBasicBlock<nb::pytorch>::id)
      .def_rw("cov", &PyBasicBlock<nb::pytorch>::cov)
      .def_rw("source", &PyBasicBlock<nb::pytorch>::source);

  nb::class_<PyBasicBlock<nb::numpy>>(m, "NumpyBasicBlock")
      .def(nb::init<>())
      .def_rw("edges", &PyBasicBlock<nb::numpy>::edges)
      .def_rw("nodes", &PyBasicBlock<nb::numpy>::nodes)
      .def_rw("features", &PyBasicBlock<nb::numpy>::features)
      .def_rw("measured_cycles", &PyBasicBlock<nb::numpy>::measuredCycles)
      .def_rw("has_virtual_root", &PyBasicBlock<nb::numpy>::hasVirtualRoot)
      .def_rw("id", &PyBasicBlock<nb::numpy>::id)
      .def_rw("cov", &PyBasicBlock<nb::numpy>::cov)
      .def_rw("source", &PyBasicBlock<nb::numpy>::source);

  m.def("load_pytorch_dataset", &loadDataset<nb::pytorch>, "path"_a,
        "start_opcode"_a, "pad_opcode"_a);

  m.def("load_numpy_dataset", &loadDataset<nb::numpy>, "path"_a,
        "start_opcode"_a, "pad_opcode"_a);
}
