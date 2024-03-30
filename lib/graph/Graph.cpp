#include "llvm-ml/graph/Graph.hpp"

#include "llvm/MC/MCInst.h"

llvm_ml::Graph llvm_ml::convertMCInstructionsToGraph(
    llvm_ml::MLTarget &mlTarget, llvm::ArrayRef<llvm::MCInst> instructions,
    const std::string &source, size_t maxOpcodes, bool addVirtualRoot,
    bool inOrderLinks) {
  Graph graph;
  graph.source = std::move(source);
  graph.hasVirtualRoot = addVirtualRoot;
  graph.maxOpcodes = maxOpcodes;

  if (addVirtualRoot) {
    NodeFeatures features;
    features.isVirtualRoot = true;
    features.opcode = 0;
    features.nodeId = 0;

    graph.addNode(features);
  }

  for (size_t i = 0; i < instructions.size(); i++) {
    // Extract the opcode of the instruction and add it as a node feature
    NodeFeatures features;
    features.opcode = instructions[i].getOpcode();
    features.isLoad = mlTarget.isMemLoad(instructions[i]);
    features.isStore = mlTarget.isMemStore(instructions[i]);
    features.isBarrier = mlTarget.isBarrier(instructions[i]);
    features.isVector = mlTarget.isVector(instructions[i]);
    features.isCompute = mlTarget.isCompute(instructions[i]);

    size_t idx = i + static_cast<size_t>(addVirtualRoot == true);

    features.nodeId = idx;

    graph.addNode(features);
    EdgeFeatures ef;
    if ((i > 0) && inOrderLinks) {
      graph.addEdge(idx - 1, idx, ef);
    }
    if (addVirtualRoot) {
      graph.addEdge(0, idx, ef);
    }
  }

  std::unordered_map<unsigned, size_t> lastWrite;

  for (size_t i = 0; i < instructions.size(); i++) {
    auto readRegs = mlTarget.getReadRegisters(instructions[i]);
    auto writeRegs = mlTarget.getWriteRegisters(instructions[i]);

    for (unsigned reg : readRegs) {
      size_t offset = addVirtualRoot == true ? 1 : 0;
      EdgeFeatures ef;
      ef.isData = true;
      ef.isImplicit = mlTarget.isImplicitReg(instructions[i], reg);
      ef.isVector = mlTarget.isVectorReg(reg);
      ef.isTile = mlTarget.isTileReg(reg);
      
      if (lastWrite.count(reg)) {
        graph.addEdge(lastWrite.at(reg) + offset, i + offset, ef);
      } else if (writeRegs.count(reg)) {
        graph.addEdge(i + offset, i + offset, ef);
      }
    }

    for (unsigned reg : writeRegs) {
      lastWrite[reg] = i;
    }
  }

  return graph;
}
