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

#include <iostream>

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
  nb::ndarray<Target, int> edges;
};

template <typename Target>
std::vector<PyBasicBlock<Target>> loadDataset(const std::string &path,
                                              bool undirected) {
  std::vector<PyBasicBlock<Target>> result;

  const auto reader = [&result,
                       undirected](llvm_ml::MCDataset::Reader &dataset) {
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
        std::vector<int> edges;
      };

      Container *container = new Container();
      nb::capsule owner(container,
                        [](void *p) noexcept { delete (Container *)p; });

      const size_t edgeScale = [undirected]() {
        if (undirected)
          return 4;
        return 2;
      }();

      container->nodes.reserve(graph.getNodes().size());
      container->edges.reserve(graph.getEdges().size() * edgeScale);

      std::transform(graph.getNodes().begin(), graph.getNodes().end(),
                     std::back_inserter(container->nodes),
                     [](const auto &node) { return node.getOpcode(); });

      const size_t numEdges = [&]() {
        size_t res = 0;
        for (const auto &edge : graph.getEdges()) {
          container->edges.push_back(edge.getFrom());
          container->edges.push_back(edge.getTo());

          res++;

          if (undirected && edge.getFrom() != edge.getTo()) {
            container->edges.push_back(edge.getTo());
            container->edges.push_back(edge.getFrom());
            res++;
          }
        }

        return res;
      }();

      std::array<size_t, 1> nodesShape{graph.getNodes().size()};
      std::array<size_t, 2> edgesShape{numEdges, 2};
      bb.nodes = nb::ndarray<Target, int>(container->nodes.data(), 1,
                                          nodesShape.data(), owner);
      bb.edges = nb::ndarray<Target, int>(container->edges.data(), 2,
                                          edgesShape.data(), owner);

      result.push_back(bb);
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
      .def_rw("measured_cycles", &PyBasicBlock<nb::pytorch>::measuredCycles)
      .def_rw("has_virtual_root", &PyBasicBlock<nb::pytorch>::hasVirtualRoot)
      .def_rw("id", &PyBasicBlock<nb::pytorch>::id)
      .def_rw("cov", &PyBasicBlock<nb::pytorch>::cov)
      .def_rw("source", &PyBasicBlock<nb::pytorch>::source);

  nb::class_<PyBasicBlock<nb::numpy>>(m, "NumpyBasicBlock")
      .def(nb::init<>())
      .def_rw("edges", &PyBasicBlock<nb::numpy>::edges)
      .def_rw("nodes", &PyBasicBlock<nb::numpy>::nodes)
      .def_rw("measured_cycles", &PyBasicBlock<nb::numpy>::measuredCycles)
      .def_rw("has_virtual_root", &PyBasicBlock<nb::numpy>::hasVirtualRoot)
      .def_rw("id", &PyBasicBlock<nb::numpy>::id)
      .def_rw("cov", &PyBasicBlock<nb::numpy>::cov)
      .def_rw("source", &PyBasicBlock<nb::numpy>::source);

  m.def("load_pytorch_dataset", &loadDataset<nb::pytorch>, "path"_a,
        "undirected"_a = false);

  m.def("load_numpy_dataset", &loadDataset<nb::numpy>, "path"_a,
        "undirected"_a = false);
}
