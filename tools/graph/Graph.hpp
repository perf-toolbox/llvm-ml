//===--- Graph.hpp - Internal graph representation ------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/target/Target.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/MC/MCInstrInfo.h"

#include <string>
#include <vector>

namespace llvm_ml {
struct NodeFeatures {
  uint32_t opcode = 0; ///< Opcode of the instruction
  bool isLoad = false; ///< true if the instruction performs global memory load
  bool isStore =
      false; ///< true if the instruction performs global memory store
  bool isBarrier = false; ///< true if the instruction emits execution barrier
  bool isAtomic = false;  ///< true if instruction is an atomic instruction
  bool isVector = false;  ///< true if the instruction is a vector instruction
  bool isCompute =
      false; ///< true if the instruction performs any kind of computation
             /// except for memory move
  bool isFloat =
      false; ///< true if the instruction performs floating point computation
  bool isVirtualRoot = false;
  size_t nodeId = 0;
};

struct EdgeFeatures {
  bool isData = false;
};

struct Graph {
  size_t maxOpcodes;
  bool hasVirtualRoot;
  std::string source;

  void addNode(const NodeFeatures &features) { mNodes.push_back(features); }

  void addEdge(size_t from, size_t to, const EdgeFeatures &features) {
    mEdges.emplace_back(from, to, features);
  }

  const std::vector<NodeFeatures> &getNodes() const { return mNodes; }

  const auto &getEdges() const { return mEdges; }

  bool operator==(const Graph &other) const {
    if ((mNodes.size() != other.mNodes.size()) ||
        (mEdges.size() != other.mEdges.size()))
      return false;

    for (size_t i = 0; i < mNodes.size(); i++) {
      if (mNodes[i].opcode != other.mNodes[i].opcode)
        return false;
    }

    for (size_t i = 0; i < mEdges.size(); i++) {
      if ((std::get<0>(mEdges[i]) != std::get<0>(other.mEdges[i])) ||
          (std::get<1>(mEdges[i]) != std::get<1>(other.mEdges[i])))
        return false;
    }

    return true;
  }

private:
  std::vector<NodeFeatures> mNodes;
  std::vector<std::tuple<size_t, size_t, EdgeFeatures>> mEdges;
};

Graph convertMCInstructionsToGraph(llvm_ml::MLTarget &mlTarget,
                                   llvm::ArrayRef<llvm::MCInst> instructions,
                                   const std::string &source, size_t maxOpcodes,
                                   bool addVirtualRoot, bool inOrderLinks);
} // namespace llvm_ml
