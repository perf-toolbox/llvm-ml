//===--- llvm-mc-extract.cpp - Extract basic blocks from binary -----------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/graph/Graph.hpp"
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
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TargetParser/Host.h"

#include <filesystem>
#include <future>
#include <indicators/indicators.hpp>
#include <llvm/Support/Threading.h>
#include <optional>

using namespace llvm;
namespace fs = std::filesystem;

cl::OptionCategory ToolOptions("llvm-mc-extract specific options");

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input file>"), cl::cat(ToolOptions));

static cl::opt<std::string> OutputDirectory("asm-dir", cl::desc("Directory to store assembly files"),
                                            cl::Required, cl::cat(ToolOptions));

static cl::opt<std::string> Prefix("prefix", cl::desc("Result files prefix"),
                                   cl::Required, cl::cat(ToolOptions));

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to assemble for, "
                              "see -version for available targets"));

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple to assemble for, "
                                  "see -version for available targets"));

static cl::opt<bool> Postprocess(
    "postprocess",
    cl::desc(
        "Apply postprocessing: remove move-only blocks, empty blocks, etc"), cl::cat(ToolOptions));
static cl::opt<bool>
    PostprocessOnly("postprocess-only",
                    cl::desc("Only run postprocessing on extracted data"), cl::cat(ToolOptions));

static llvm::mc::RegisterMCTargetOptionsFlags MOF;

static void clearTerminalColors() {
  indicators::show_console_cursor(true);
  std::cout << termcolor::reset;
}

static void signalHandler(void *) { clearTerminalColors(); }

static void interruptHandler() { clearTerminalColors(); }

static const Target *getTarget(const object::ObjectFile *Obj) {
  // Figure out the target triple.
  Triple TheTriple("unknown-unknown-unknown");
  if (TripleName.empty()) {
    TheTriple = Obj->makeTriple();
  } else {
    TheTriple.setTriple(Triple::normalize(TripleName));
    auto Arch = Obj->getArch();
    if (Arch == Triple::arm || Arch == Triple::armeb)
      Obj->setARMSubArch(TheTriple);
  }

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget =
      TargetRegistry::lookupTarget(ArchName, TheTriple, Error);
  if (!TheTarget)
    errs() << Obj->getFileName() << " can't find target: " + Error;

  // Update the triple name and return the found target.
  TripleName = TheTriple.getTriple();
  return TheTarget;
}

static void extractBasicBlocks(const object::ObjectFile &object,
                               const Target *target, Triple triple) {
  std::string cpu = object.tryGetCPUName().value_or("").str();
  Expected<SubtargetFeatures> featuresOrError = object.getFeatures();
  if (!featuresOrError)
    std::terminate();
  SubtargetFeatures features = *featuresOrError;

  const MCTargetOptions options;
  std::unique_ptr<MCRegisterInfo> mcri(
      target->createMCRegInfo(triple.getTriple()));
  std::unique_ptr<MCAsmInfo> mcai(
      target->createMCAsmInfo(*mcri, triple.getTriple(), options));
  std::unique_ptr<MCSubtargetInfo> msti(target->createMCSubtargetInfo(
      triple.getTriple(), cpu, features.getString()));
  std::unique_ptr<MCInstrInfo> mcii(target->createMCInstrInfo());

  MCContext context(triple, mcai.get(), mcri.get(), msti.get());
  std::unique_ptr<MCObjectFileInfo> mcofi(
      target->createMCObjectFileInfo(context, false));
  context.setObjectFileInfo(mcofi.get());

  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  std::unique_ptr<MCDisassembler> disasm(
      target->createMCDisassembler(*msti, context));

  if (!disasm) {
    // TODO proper error handling
    errs() << "Failed to create disassembler for " << triple.getTriple()
           << "\n";
    std::terminate();
  }

  unsigned asmVariant = mcai->getAssemblerDialect();
  std::unique_ptr<MCInstPrinter> instPrinter(
      target->createMCInstPrinter(triple, asmVariant, *mcai, *mcii, *mcri));
  instPrinter->setPrintImmHex(false);

  const auto isBlockTerminator = [&](const MCInst &inst) {
    const MCInstrDesc &desc = mcii->get(inst.getOpcode());
    return desc.isTerminator() || desc.isCall() || mlTarget->isSyscall(inst);
  };

  uint64_t blockCounter = 0;

  std::vector<std::unique_ptr<indicators::BlockProgressBar>> owningBars;
  indicators::DynamicProgress<indicators::BlockProgressBar> bars;
  for (const auto &s : llvm::enumerate(object.sections())) {
    if (s.value().getSize() == 0)
      continue;

    using namespace indicators;
    owningBars.emplace_back(new indicators::BlockProgressBar{
        option::BarWidth{80}, option::ForegroundColor{Color::green},
        option::FontStyles{
            std::vector<indicators::FontStyle>{indicators::FontStyle::bold}},
        option::PrefixText{"Section " + std::to_string(s.index())}});

    bars.push_back(*owningBars.back());
  }

  bars.set_option(indicators::option::HideBarWhenComplete{false});

  size_t curSection = 0;
  for (const auto &section : object.sections()) {
    if (!section.isText() || section.isVirtual())
      continue;

    auto contentsOrErr = section.getContents();
    if (!contentsOrErr)
      continue;

    const uint64_t sectionAddress = section.getAddress();
    const uint64_t sectionSize = section.getSize();

    if (sectionSize == 0)
      continue;

    ArrayRef<uint8_t> bytes(
        reinterpret_cast<const uint8_t *>(contentsOrErr.get().data()),
        sectionSize);

    const auto getPath = [&]() {
      SmallVector<char, 128> path;
      sys::path::append(path, OutputDirectory,
                        (Twine(Prefix) + Twine(blockCounter) + ".s").str());
      return std::string(path.begin(), path.end());
    };

    std::error_code EC;
    auto os = std::make_unique<raw_fd_ostream>(getPath(), EC);
    if (EC) {
      errs() << "Failed to create a file: " << EC.message();
      std::terminate();
    }

    indicators::show_console_cursor(false);

    uint64_t index = 0;
    while (index < sectionSize) {
      MCInst inst;
      uint64_t instSize = 0;

      if (disasm->getInstruction(inst, instSize, bytes.slice(index),
                                 sectionAddress + index, errs())) {
        if (isBlockTerminator(inst)) {
          blockCounter++;
          os->close();
          os = std::make_unique<raw_fd_ostream>(getPath(), EC);
          if (EC) {
            errs() << "Failed to create a file: " << EC.message();
            std::terminate();
          }
        } else if (!mlTarget->isNop(inst)) {
          instPrinter->printInst(&inst, sectionAddress + index, "", *msti, *os);
          *os << "\n";
        }

        index += instSize;
      } else {
        errs() << "Failed to parse block\n";
        break;
      }

      bars[curSection].set_option(indicators::option::PostfixText{
          std::to_string(index) + "/" + std::to_string(sectionSize)});
      int percent = static_cast<int>(
          (static_cast<float>(index) / static_cast<float>(sectionSize)) *
          100.f);
      bars[curSection].set_progress(percent);
    }
    bars[curSection].set_progress(100);
    bars[curSection].mark_as_completed();
    curSection++;
    indicators::show_console_cursor(true);
  }
}

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

static void postprocessSingleFile(
    const fs::path &path, std::mutex &lock,
    std::vector<std::pair<fs::path, std::optional<llvm_ml::Graph>>> &graphs) {
  const llvm::Target *target = getTarget("");
  Triple triple(TripleName);

  const llvm::MCTargetOptions options =
      llvm::mc::InitMCTargetOptionsFromFlags();
  std::unique_ptr<llvm::MCRegisterInfo> mcri(
      target->createMCRegInfo(TripleName));
  std::unique_ptr<llvm::MCAsmInfo> mcai(
      target->createMCAsmInfo(*mcri, TripleName, options));
  std::unique_ptr<llvm::MCSubtargetInfo> msti(
      target->createMCSubtargetInfo(TripleName, "", ""));
  std::unique_ptr<llvm::MCInstrInfo> mcii(target->createMCInstrInfo());

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer =
      llvm::MemoryBuffer::getFileOrSTDIN(path.c_str(), /*IsText=*/true);
  if (std::error_code EC = buffer.getError()) {
    // Remove all broken files
    fs::remove(path);
    return;
  }

  llvm::SourceMgr sourceMgr;

  // Tell SrcMgr about this buffer, which is what the parser will pick
  // up.
  sourceMgr.AddNewSourceBuffer(std::move(*buffer), llvm::SMLoc());

  llvm::MCContext context(triple, mcai.get(), mcri.get(), msti.get(),
                          &sourceMgr);
  std::unique_ptr<llvm::MCObjectFileInfo> mcofi(
      target->createMCObjectFileInfo(context, /*PIC=*/false));
  context.setObjectFileInfo(mcofi.get());

  auto instructions = llvm_ml::parseAssembly(
      sourceMgr, *mcii, *mcri, *mcai, *msti, context, target, triple, options);

  if (!instructions) {
    (void)instructions.takeError();
    fs::remove(path);
    return;
  }

  if (instructions->size() < 2) {
    fs::remove(path);
    return;
  }

  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  bool hasCompute =
      std::any_of(instructions->begin(), instructions->end(),
                  [&](const llvm::MCInst &inst) {
                    return !mlTarget->isMemLoad(inst) &&
                           !mlTarget->isMemStore(inst) &&
                           !mlTarget->isMov(inst) && !mlTarget->isLea(inst) &&
                           !mlTarget->isPush(inst) && !mlTarget->isPop(inst);
                  });

  bool hasVariableLatency = std::any_of(
      instructions->begin(), instructions->end(),
      [&](const llvm::MCInst &inst) { return mlTarget->isVarLatency(inst); });

  // This basic block is probably not doing anything useful or has a
  // variable latency
  if (!hasCompute || hasVariableLatency) {
    fs::remove(path);
    return;
  }

  std::lock_guard guard{lock};
  // Intentionally prevent source saving, virtual roots or in-order links to
  // save some memory.
  graphs.emplace_back(path, llvm_ml::convertMCInstructionsToGraph(
                                *mlTarget, *instructions, "", 0, false, false));
}

static void deduplicate(
    std::vector<std::pair<fs::path, std::optional<llvm_ml::Graph>>> &graphs) {
  using namespace indicators;

  size_t duplicates = 0;

  llvm::outs() << "Removing duplicates...\n";
  BlockProgressBar bar{
      option::BarWidth{80}, option::ForegroundColor{Color::green},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::MaxProgress{graphs.size()}};

  for (size_t i = 0; i < graphs.size(); i++) {
    if (!graphs[i].second.has_value()) {
      continue;
    }
    for (size_t j = i + 1; j < graphs.size(); j++) {
      if (!graphs[j].second.has_value())
        continue;

      if (*graphs[i].second == *graphs[j].second) {
        fs::remove(graphs[j].first);
        graphs[j].second = std::nullopt;
        duplicates++;
      }
    }

    bar.tick();
  }

  llvm::outs() << "Found " << duplicates << " duplicates!\n";
}

static void postprocess() {
  using namespace indicators;
  indicators::show_console_cursor(false);
  const fs::path blocks_dir{OutputDirectory.c_str()};

  IndeterminateProgressBar spinner{
      option::BarWidth{80},
      option::Start{"["},
      option::Fill{"·"},
      option::Lead{"<==>"},
      option::End{"]"},
      option::PostfixText{"Collecting files..."},
      option::ForegroundColor{indicators::Color::yellow},
      option::FontStyles{
          std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}};

  llvm::ThreadPoolStrategy strategy = llvm::hardware_concurrency();
  const unsigned int numThreads = strategy.compute_thread_count();
  llvm::ThreadPool pool{strategy};

  auto filenames = pool.async([&]() {
    std::vector<fs::path> bbs;
    bbs.reserve(10'000'000);

    for (const auto &entry : fs::directory_iterator(blocks_dir)) {
      auto path = entry.path();
      if (!fs::is_regular_file(path) || path.extension() != ".s")
        continue;

      bbs.push_back(path);
    }

    spinner.mark_as_completed();

    return bbs;
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

  BlockProgressBar bar{
      option::BarWidth{80}, option::ForegroundColor{Color::green},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::MaxProgress{files.size()}};

  std::mutex parsedGraphLock;
  std::vector<std::pair<fs::path, std::optional<llvm_ml::Graph>>> graphs;

  llvm::errs() << "Running in " << numThreads << " threads...\n";
  for (const auto &path : files) {
    fs::path outFile = blocks_dir / path.filename();

    pool.async([path = outFile, &bar, &parsedGraphLock, &graphs]() {
      postprocessSingleFile(path, parsedGraphLock, graphs);
      bar.tick();
    });
  }

  pool.wait();

  deduplicate(graphs);

  indicators::show_console_cursor(true);
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeAllDisassemblers();
  InitializeAllAsmParsers();

  cl::ParseCommandLineOptions(argc, argv,
                              "extract asm basic blocks from binary\n");

  sys::AddSignalHandler(signalHandler, nullptr);
  sys::SetInterruptFunction(interruptHandler);

  sys::fs::file_status status;
  std::error_code err = sys::fs::status(OutputDirectory, status);

  if (err) {
    errs() << "Error code " << err.value() << ": " << err.message() << "\n";
    return 1;
  }

  if (!sys::fs::is_directory(status)) {
    errs() << "Provided path " << OutputDirectory << " is not a directory\n";
    return 1;
  }

  if (!PostprocessOnly) {
    if (InputFilename.empty()) {
      errs() << "No input file name was provided\n";
      return 1;
    }
    ErrorOr<std::unique_ptr<MemoryBuffer>> fileOrErr =
        MemoryBuffer::getFile(InputFilename, /*IsText=*/false);
    if (std::error_code errc = fileOrErr.getError()) {
      errs() << "Error reading file: " << errc.message() << "\n";
      return 1;
    }

    Expected<std::unique_ptr<object::ObjectFile>> objOrErr =
        object::ObjectFile::createObjectFile(fileOrErr->get()->getMemBufferRef());
    if (!objOrErr) {
      errs() << "Error creating object file: " << toString(objOrErr.takeError())
             << "\n";
      return 1;
    }

    const Target *target = getTarget(objOrErr.get().get());
    if (!target) {
      return 1;
    }

    extractBasicBlocks(*objOrErr.get(), target, Triple(TripleName));
  }

  if (Postprocess || PostprocessOnly)
    postprocess();

  return 0;
}
