//===--- llvm-mc-embedding.cpp - Convert ASM to ML embeddings -------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/graph/Graph.hpp"
#include "llvm-ml/structures/structures.hpp"
#include "llvm-ml/target/Target.hpp"

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
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TargetParser/Host.h"

#include <filesystem>
#include <indicators/indicators.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static void clearTerminalColors() {
  indicators::show_console_cursor(true);
  std::cout << termcolor::reset;
}

static void signalHandler(void *) { clearTerminalColors(); }

static void interruptHandler() { clearTerminalColors(); }

static json getMetaFeatures(const llvm_ml::NodeFeatures &n) {
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

static void storeGraphIntoJSON(json &out, const llvm_ml::Graph &g) {
  out["source"] = g.source;
  out["max_opcodes"] = g.maxOpcodes;
  out["has_virtual_root"] = g.hasVirtualRoot;
}

static void exportReadableJSON(const llvm_ml::Graph &g, llvm::raw_ostream &os) {
  auto nodes = json::array();

  for (auto n : g.getNodes()) {
    json node = getMetaFeatures(n);
    nodes.push_back(node);
  }

  auto edges = json::array();
  for (const auto &e : g.getEdges()) {
    auto edge = json();
    edge["from"] = std::get<0>(e);
    edge["to"] = std::get<1>(e);
    edges.push_back(edge);
  }

  json out;
  out["nodes"] = nodes;
  out["edges"] = edges;
  storeGraphIntoJSON(out, g);

  os << out.dump(4);
}

static void exportBinary(const llvm_ml::Graph &g, std::filesystem::path path) {
  capnp::MallocMessageBuilder message;
  llvm_ml::MCGraph::Builder graph = message.initRoot<llvm_ml::MCGraph>();
  graph.setSource(g.source);
  graph.setMaxOpcode(g.maxOpcodes);
  graph.setHasVirtualRoot(g.hasVirtualRoot);

  capnp::List<llvm_ml::MCNode>::Builder nodes =
      graph.initNodes(g.getNodes().size());

  for (const auto &n : g.getNodes()) {
    llvm_ml::MCNode::Builder bNode = nodes[n.nodeId];
    bNode.setIsLoad(n.isLoad);
    bNode.setIsStore(n.isStore);
    bNode.setIsBarrier(n.isBarrier);
    bNode.setIsAtomic(n.isAtomic);
    bNode.setIsVector(n.isVector);
    bNode.setIsCompute(n.isCompute);
    bNode.setOpcode(n.opcode);
    bNode.setIsVirtualRoot(n.isVirtualRoot);
    bNode.setNodeId(n.nodeId);
  }

  capnp::List<llvm_ml::MCEdge>::Builder edges =
      graph.initEdges(g.getEdges().size());
  for (const auto &e : llvm::enumerate(g.getEdges())) {
    llvm_ml::MCEdge::Builder bEdge = edges[e.index()];
    bEdge.setFrom(std::get<0>(e.value()));
    bEdge.setTo(std::get<1>(e.value()));
    bEdge.setIsDataDependency(
        std::get<llvm_ml::EdgeFeatures>(e.value()).isData);
  }

  llvm_ml::writeToFile(path, message);
}

static llvm::mc::RegisterMCTargetOptionsFlags MOF;

static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input file>"));

static llvm::cl::opt<std::string> OutputFilename("o",
                                                 llvm::cl::desc("output file"));

static llvm::cl::opt<std::string>
    ArchName("arch", llvm::cl::desc("Target arch to assemble for, "
                                    "see -version for available targets"));

static llvm::cl::opt<std::string>
    TripleName("triple", llvm::cl::desc("Target triple to assemble for, "
                                        "see -version for available targets"));

static llvm::cl::opt<bool>
    ReadableJSON("readable-json",
                 llvm::cl::desc("Export features as readable JSON format"));
static llvm::cl::opt<bool> VirtualRoot(
    "virtual-root",
    llvm::cl::desc(
        "Add virtual root node and connect all other nodes with it"));
static llvm::cl::opt<bool>
    InOrder("in-order",
            llvm::cl::desc("Connect nodes sequentially to form a path"));
static llvm::cl::opt<int>
    Jobs("j", llvm::cl::desc("num threads (only makes sense in batch mode)"),
         llvm::cl::init(0));

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

static llvm::Error processSingleInput(fs::path input, fs::path output,
                                      llvm::Triple triple) {
  // FIXME(Alex): this is potentially not thread safe
  const llvm::Target *target = getTarget("");

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer =
      llvm::MemoryBuffer::getFileOrSTDIN(input.c_str(), /*IsText=*/true);
  if (std::error_code EC = buffer.getError()) {
    return llvm::createStringError(EC, "Failed to open the file");
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

  auto instructions = llvm_ml::parseAssembly(
      sourceMgr, *mcii, *mcri, *mcai, *msti, context, target, triple, options);

  if (!instructions) {
    return instructions.takeError();
  }

  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  std::string source =
      sourceMgr.getMemoryBuffer(sourceMgr.getMainFileID())->getBuffer().str();

  llvm_ml::Graph graph = llvm_ml::convertMCInstructionsToGraph(
      *mlTarget, *instructions, source, mcii->getNumOpcodes(), VirtualRoot,
      InOrder);

  if (ReadableJSON) {
    std::error_code ec;
    llvm::raw_fd_ostream ofs(output.c_str(), ec);
    // TODO check ec

    exportReadableJSON(graph, ofs);
    ofs.close();
  } else {
    exportBinary(graph, output);
  }

  return llvm::Error::success();
}

int main(int argc, char **argv) {
  llvm::InitLLVM X(argc, argv);

  // Initialize targets and assembly printers/parsers.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "convert assembly to ML embeddings\n");

  llvm::sys::AddSignalHandler(signalHandler, nullptr);
  llvm::sys::SetInterruptFunction(interruptHandler);

  const llvm::Target *target = getTarget("");

  if (!target)
    return 1;
  // Now that getTarget() has (potentially) replaced TripleName, it's safe to
  // construct the Triple object.
  llvm::Triple triple(TripleName);

  fs::path input{InputFilename.c_str()};
  fs::path output{OutputFilename.c_str()};
  if (fs::is_directory(input) && fs::is_directory(output)) {
    using namespace indicators;
    indicators::show_console_cursor(false);

    llvm::ThreadPoolStrategy strategy = []() {
      if (Jobs != 0) {
        return llvm::hardware_concurrency(Jobs);
      }
      return llvm::hardware_concurrency();
    }();
    unsigned int numThreads = strategy.compute_thread_count();

    llvm::ThreadPool pool{strategy};

    IndeterminateProgressBar spinner{
        option::BarWidth{40},
        option::Start{"["},
        option::Fill{"·"},
        option::Lead{"<==>"},
        option::End{"]"},
        option::PostfixText{"Collecting files..."},
        option::ForegroundColor{indicators::Color::yellow},
        option::FontStyles{
            std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

    auto filenames = pool.async([&spinner, &input]() {
      std::vector<fs::path> files;
      // This is a very big vector, but the datasets tend to be no smaller
      files.reserve(30'000'000);

      for (const auto &entry : fs::directory_iterator(input)) {
        auto path = entry.path();
        if (!fs::is_regular_file(path) || path.extension() != ".s")
          continue;

        files.push_back(path);
      }

      spinner.mark_as_completed();

      return files;
    });

    while (!spinner.is_completed()) {
      spinner.tick();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    filenames.wait();
    const std::vector<fs::path> &files = filenames.get();

    spinner.set_option(option::ForegroundColor{Color::green});
    spinner.set_option(option::PrefixText{"✔"});
    spinner.set_option(option::PostfixText{"Complete!"});

    std::vector<std::shared_future<llvm::Error>> dispatchedTasks;
    dispatchedTasks.reserve(numThreads);

    BlockProgressBar bar{
        option::BarWidth{80}, option::ForegroundColor{Color::green},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
        option::MaxProgress{files.size()}};

    llvm::errs() << "Running in " << numThreads << " threads...\n";
    for (const auto &path : files) {
      fs::path outFile = output / path.filename();
      if (ReadableJSON) {
        outFile.replace_extension("json");
      } else {
        outFile.replace_extension("cbuf");
      }

      auto future = pool.async(&processSingleInput, std::move(path),
                               std::move(outFile), triple);
      dispatchedTasks.push_back(std::move(future));

      if (dispatchedTasks.size() == numThreads) {
        for (auto &future : dispatchedTasks)
          future.wait();

        for (auto &f : dispatchedTasks) {
          // TODO(Alex): this is definitely a UB
          if (const_cast<llvm::Error &>(f.get())) {
            llvm::errs() << f.get() << "\n";
          }
        }

        dispatchedTasks.clear();
      }

      bar.tick();
    }

    indicators::show_console_cursor(true);
  } else if (fs::is_regular_file(input) && !fs::is_regular_file(output)) {
    auto err = processSingleInput(input, output, triple);
    if (err) {
      llvm::errs() << err << "\n";
      return 1;
    }
  } else {
    llvm::errs() << "Both input and output must either be directories (batch "
                    "mode) or files\n";
    return 1;
  }

  return 0;
}
