//===--- llvm-mc-embedding.cpp - Convert ASM to ML embeddings -------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "target/Target.hpp"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectStreamer.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TargetParser/Host.h"

#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using json = nlohmann::json;

struct NodeFeatures {
  uint32_t opcode = 0; ///< Opcode of the instruction
  bool isLoad = false; ///< true if the instruction performs global memory load
  bool isStore =
      false; ///< true if the instruction performs global memory store
  bool isBarrier = false; ///< true if the instruction emits execution barrier
  bool isAtomic = false;  ///< true if instruction is an atomic instruction
  bool isVector = false;  ///< true if the instruction is a vector instruction
  bool isCompute = false; ///< true if the instruction performs any kind of computation
                          /// except for memory move
  bool isVirtualRoot = false;
  size_t nodeId = 0;

  std::vector<int8_t>
  get_one_hot_embeddings(const std::map<unsigned, size_t> &map) const {
    std::vector<int8_t> vec;
    vec.resize(map.size());
    if (!isVirtualRoot)
      vec[map.at(opcode)] = 1;
    return vec;
  }

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /* version */) {
    ar &opcode;
  }
};

struct EdgeFeatures {
private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /* version */) {}
};

struct GraphProperties {
  size_t numOpcodes;
  bool hasVirtualRoot;
  std::string source;

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /* version */) {
    ar &numOpcodes;
    ar &source;
  }
};

using Graph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                          NodeFeatures, EdgeFeatures, GraphProperties>;

static json getMetaFeatures(const NodeFeatures &n) {
  json node;
  node["is_load"] = n.isLoad;
  node["is_store"] = n.isStore;
  node["is_barrier"] = n.isBarrier;
  node["is_atomic"] = n.isAtomic;
  node["is_vector"] = n.isVector;
  node["is_compute"] = n.isCompute;
  node["opcode"] = n.opcode;

  return node;
}

static void storeGraphIntoJSON(json &out, const Graph &g) {
  out["source"] = boost::get_property(g, &GraphProperties::source);
  out["num_opcodes"] = boost::get_property(g, &GraphProperties::numOpcodes);
  out["has_virtual_root"] =
      boost::get_property(g, &GraphProperties::hasVirtualRoot);
}

static void exportReadableJSON(const Graph &g, llvm::raw_ostream &os) {
  auto nodes = json::array();

  for (auto vd : boost::make_iterator_range(vertices(g))) {
    const auto &n = g[vd];
    json node = getMetaFeatures(n);
    nodes.push_back(node);
  }

  auto edges = json::array();
  for (const auto &e : boost::make_iterator_range(boost::edges(g))) {
    auto edge = json({boost::source(e, g), boost::target(e, g)});
    edges.push_back(edge);
  }

  json out;
  out["nodes"] = nodes;
  out["edges"] = edges;
  storeGraphIntoJSON(out, g);

  os << out.dump(4);
}

static void exportJSON(const Graph &g, const std::map<unsigned, size_t> &map,
                       llvm::raw_ostream &os) {
  auto nodes = json::array();
  auto nodes_meta = json::array();
  for (auto vd : boost::make_iterator_range(vertices(g))) {
    const auto &n = g[vd];
    nodes.push_back(n.get_one_hot_embeddings(map));

    json meta = getMetaFeatures(n);
    nodes_meta.push_back(meta);
  }

  auto edges = json::array();
  for (const auto &e : boost::make_iterator_range(boost::edges(g))) {
    auto edge = json({boost::source(e, g), boost::target(e, g)});
    edges.push_back(edge);
  }

  json out;
  out["nodes"] = nodes;
  out["meta"] = nodes_meta;
  out["edges"] = edges;
  storeGraphIntoJSON(out, g);

  os << out.dump();
}

static llvm::mc::RegisterMCTargetOptionsFlags MOF;

static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input file>"),
                                                llvm::cl::init("-"));

static llvm::cl::opt<std::string>
    OutputFilename("o", llvm::cl::desc("output file"), llvm::cl::init("-"));

static llvm::cl::opt<std::string>
    ArchName("arch", llvm::cl::desc("Target arch to assemble for, "
                                    "see -version for available targets"));

static llvm::cl::opt<std::string>
    TripleName("triple", llvm::cl::desc("Target triple to assemble for, "
                                        "see -version for available targets"));

static llvm::cl::opt<bool>
    ReadableJSON("readable-json",
                 llvm::cl::desc("Export features as readable JSON format"));
static llvm::cl::opt<bool> Dot("dot",
                               llvm::cl::desc("Visualize graph in DOT format"));
static llvm::cl::opt<bool> VirtualRoot(
    "virtual-root",
    llvm::cl::desc(
        "Add virtual root node and connect all other nodes with it"));
static llvm::cl::opt<bool>
    InOrder("in-order",
            llvm::cl::desc("Connect nodes sequentially to form a path"));

static const llvm::Target *getTarget(const char *ProgName) {
  // Figure out the target triple.
  if (TripleName.empty())
    TripleName = llvm::sys::getDefaultTargetTriple();

  llvm::Triple triple(llvm::Triple::normalize(TripleName));

  // Get the target specific parser.
  std::string Error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(ArchName, triple, Error);
  if (!target) {
    return nullptr;
  }

  // Update the triple name and return the found target.
  TripleName = triple.getTriple();
  return target;
}

namespace {
class MCStreamerWrapper final : public llvm::MCStreamer {
  std::vector<llvm::MCInst> &instrs;

public:
  MCStreamerWrapper(llvm::MCContext &Context, std::vector<llvm::MCInst> &instrs)
      : MCStreamer(Context), instrs(instrs) {}

  // We only want to intercept the emission of new instructions.
  void emitInstruction(const llvm::MCInst &inst,
                       const llvm::MCSubtargetInfo & /* unused */) override {
    instrs.push_back(inst);
  }

  bool emitSymbolAttribute(llvm::MCSymbol *, llvm::MCSymbolAttr) override {
    return true;
  }

  void emitCommonSymbol(llvm::MCSymbol *, uint64_t /*size*/,
                        llvm::Align) override {}
  void emitZerofill(llvm::MCSection *, llvm::MCSymbol *symbol = nullptr,
                    uint64_t size = 0,
                    llvm::Align byteAlignment = llvm::Align(1),
                    llvm::SMLoc loc = llvm::SMLoc()) override {}
  void emitGPRel32Value(const llvm::MCExpr *) override {}
  void beginCOFFSymbolDef(const llvm::MCSymbol *) override {}
  void emitCOFFSymbolStorageClass(int /*storageClass*/) override {}
  void emitCOFFSymbolType(int /*type*/) override {}
  void endCOFFSymbolDef() override {}
};
} // namespace

llvm::Expected<std::vector<llvm::MCInst>>
parseAssembly(llvm::SourceMgr &srcMgr, const llvm::MCInstrInfo &mcii,
              const llvm::MCRegisterInfo &mcri, const llvm::MCAsmInfo &mcai,
              const llvm::MCSubtargetInfo &msti, llvm::MCContext &context,
              const llvm::Target *target, const llvm::Triple &triple,
              llvm::MCTargetOptions options) {

  std::vector<llvm::MCInst> instructions;

  MCStreamerWrapper streamer(context, instructions);

  auto parser = llvm::createMCAsmParser(srcMgr, context, streamer, mcai);

  std::unique_ptr<llvm::MCTargetAsmParser> target_parser(
      target->createMCAsmParser(msti, *parser, mcii, options));

  if (!target_parser)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to create target parser");

  parser->setTargetParser(*target_parser);

  int parse_result = parser->Run(false);
  if (parse_result)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to parse assembly");

  return instructions;
}

static std::map<unsigned, size_t> getOpcodeMap(llvm::MCInstrInfo &mcii) {
  size_t opcodeId = 0;
  std::map<unsigned, size_t> map;

  for (unsigned opcode = 0; opcode < mcii.getNumOpcodes(); opcode++) {
    const llvm::MCInstrDesc &desc = mcii.get(opcode);

    if (desc.isPseudo()) {
      continue;
    }

    map[opcode] = opcodeId;
    opcodeId++;
  }

  return map;
}

int main(int argc, char **argv) {
  llvm::InitLLVM X(argc, argv);

  // Initialize targets and assembly printers/parsers.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "convert assembly to ML embeddings\n");

  const llvm::Target *target = getTarget("");

  if (!target)
    return 1;
  // Now that getTarget() has (potentially) replaced TripleName, it's safe to
  // construct the Triple object.
  llvm::Triple triple(TripleName);

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer =
      llvm::MemoryBuffer::getFileOrSTDIN(InputFilename, /*IsText=*/true);
  if (std::error_code EC = buffer.getError()) {
    return 1;
  }

  const llvm::MCTargetOptions options =
      llvm::mc::InitMCTargetOptionsFromFlags();
  std::unique_ptr<llvm::MCRegisterInfo> mcri(
      target->createMCRegInfo(TripleName));
  std::unique_ptr<llvm::MCAsmInfo> mcai(
      target->createMCAsmInfo(*mcri, TripleName, options));
  std::unique_ptr<llvm::MCSubtargetInfo> msti(
      target->createMCSubtargetInfo(TripleName, "", ""));
  std::unique_ptr<llvm::MCInstrInfo> mcii(target->createMCInstrInfo());

  llvm::SourceMgr sourceMgr;

  // Tell SrcMgr about this buffer, which is what the parser will pick up.
  sourceMgr.AddNewSourceBuffer(std::move(*buffer), llvm::SMLoc());

  llvm::MCContext context(triple, mcai.get(), mcri.get(), msti.get(),
                          &sourceMgr);
  std::unique_ptr<llvm::MCObjectFileInfo> mcofi(
      target->createMCObjectFileInfo(context, /*PIC=*/false));
  context.setObjectFileInfo(mcofi.get());

  auto instructions = parseAssembly(sourceMgr, *mcii, *mcri, *mcai, *msti,
                                    context, target, triple, options);

  if (!instructions) {
    return 1;
  }

  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  auto map = getOpcodeMap(*mcii);

  std::string source =
      sourceMgr.getMemoryBuffer(sourceMgr.getMainFileID())->getBuffer().str();
  GraphProperties gp;
  gp.source = std::move(source);
  gp.hasVirtualRoot = VirtualRoot;
  gp.numOpcodes = map.size();

  Graph g(instructions->size() + static_cast<size_t>(VirtualRoot == true), gp);

  if (VirtualRoot) {
    NodeFeatures features;
    features.isVirtualRoot = true;
    features.opcode = 0;
    features.nodeId = 0;

    boost::put(boost::vertex_bundle, g, 0, features);
  }

  for (size_t i = 0; i < instructions->size(); i++) {
    // Extract the opcode of the instruction and add it as a node feature
    NodeFeatures features;
    features.opcode = (*instructions)[i].getOpcode();
    features.isLoad = mlTarget->isMemLoad((*instructions)[i]);
    features.isStore = mlTarget->isMemStore((*instructions)[i]);
    features.isBarrier = mlTarget->isBarrier((*instructions)[i]);
    features.isVector = mlTarget->isVector((*instructions)[i]);
    features.isCompute = mlTarget->isCompute((*instructions)[i]);

    size_t idx = i + static_cast<size_t>(VirtualRoot == true);

    features.nodeId = idx;

    boost::put(boost::vertex_bundle, g, idx, features);
    if ((i > 0) && InOrder) {
      boost::add_edge(idx - 1, idx, g);
    }
    if (VirtualRoot) {
      boost::add_edge(0, idx, g);
    }
  }

  std::unordered_map<unsigned, size_t> lastWrite;

  for (size_t i = 0; i < instructions->size(); i++) {
    auto readRegs = mlTarget->getReadRegisters((*instructions)[i]);
    auto writeRegs = mlTarget->getReadRegisters((*instructions)[i]);

    for (unsigned reg : readRegs) {
      if (lastWrite.count(reg)) {
        size_t offset = static_cast<size_t>(VirtualRoot == true);
        boost::add_edge(lastWrite.at(reg) + offset, i + offset, g);
      }
    }

    for (unsigned reg : writeRegs) {
      lastWrite[reg] = i;
    }
  }

  std::error_code ec;
  llvm::raw_fd_ostream ofs(OutputFilename, ec);

  if (ReadableJSON) {
    exportReadableJSON(g, ofs);
  } else if (Dot) {
    std::ostringstream os;
    boost::dynamic_properties dp;
    dp.property("label", get(&NodeFeatures::opcode, g));
    dp.property("node_id", get(&NodeFeatures::nodeId, g));
    boost::write_graphviz_dp(os, g, dp);
    os.flush();
    ofs << os.str();
  } else {
    exportJSON(g, map, ofs);
  }

  ofs.close();

  return 0;
}
