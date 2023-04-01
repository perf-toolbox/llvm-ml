//===--- llvm-mcembedding.cpp - Convert ASM to ML embeddings --------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

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

#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <fstream>
#include <iostream>

struct NodeFeatures {
  uint32_t opcode; ///< Opcode of the instruction

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int /* version */) {
    ar &opcode;
  }
};

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

  using Graph = boost::adjacency_list<boost::vecS, boost::vecS,
                                      boost::directedS, NodeFeatures>;

  Graph g(instructions->size());

  // Extract the node and edge features and add them to the graph
  for (int i = 0; i < instructions->size(); i++) {
    // Extract the opcode of the instruction and add it as a node feature
    NodeFeatures node_feat;
    node_feat.opcode = (*instructions)[i].getOpcode();
    boost::put(boost::vertex_bundle, g, i, node_feat);

    // Extract the latency between the current instruction and its successors
    // and add it as an edge feature
    for (int j = i + 1; j < instructions->size(); j++) {
      // TODO do we need to add any edge features here?
      boost::add_edge(i, j, g);
    }
  }

  std::ofstream ofs(OutputFilename.c_str());
  boost::archive::text_oarchive oa(ofs);

  oa << g;

  ofs.close();

  return 0;
}
