//===--- llvm-mc-bench.cpp - Benchmark ASM basic blocks -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkGenerator.hpp"
#include "BenchmarkResult.hpp"
#include "BenchmarkRunner.hpp"
#include "counters.hpp"
#include "llvm-ml/target/Target.hpp"

#include "llvm/AsmParser/Parser.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Module.h"
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
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

#include <filesystem>
#include <indicators/indicators.hpp>

namespace fs = std::filesystem;
using namespace llvm;

static mc::RegisterMCTargetOptionsFlags MOF;

cl::OptionCategory ToolOptions("llvm-mc-bench specific options");

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input file or directory>"),
                                          cl::cat(ToolOptions));

static cl::opt<std::string> OutputFilename("o", cl::desc("output file"),
                                           cl::Required, cl::cat(ToolOptions));

static cl::opt<int>
    NumRepeat("num-repeat",
              cl::desc("number of basic block repititions for benchmark run"),
              cl::init(0), cl::cat(ToolOptions));
static cl::opt<int> NumRepeatNoise(
    "num-repeat-noise",
    cl::desc("number of basic block repititions for noise measurement"),
    cl::init(10), cl::cat(ToolOptions));

static cl::opt<int>
    NumMaxRuns("r", cl::desc("maximum number of test harness re-runs"),
               cl::init(50), cl::cat(ToolOptions));

static cl::list<int>
    PinnedCPUs("c", cl::desc("IDs of the CPU cores to pin this process to"),
               cl::Required, cl::cat(ToolOptions));

static cl::opt<bool>
    ReadableJSON("readable-json",
                 cl::desc("export measurements to a JSON file"), cl::init(0),
                 cl::cat(ToolOptions));

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to assemble for, "
                              "see -version for available targets"));

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple to assemble for, "
                                  "see -version for available targets"));

static cl::opt<std::string>
    LogFile("log-file", cl::desc("Path to a file to log errors in batch mode"),
            cl::cat(ToolOptions));

static const Target *getTarget() {
  // Figure out the target triple.
  if (TripleName.empty())
    TripleName = sys::getDefaultTargetTriple();

  Triple triple(Triple::normalize(TripleName));

  // Get the target specific parser.
  std::string Error;
  const Target *target = TargetRegistry::lookupTarget(ArchName, triple, Error);
  if (!target) {
    return nullptr;
  }

  // Update the triple name and return the found target.
  TripleName = triple.getTriple();
  return target;
}

llvm::Error runSingleFile(fs::path input, fs::path output,
                          const llvm::Target *target, int numRepeat,
                          int numNoiseRepeat, int pinnedCPU) {
  Triple triple(TripleName);

  ErrorOr<std::unique_ptr<MemoryBuffer>> buffer =
      MemoryBuffer::getFileOrSTDIN(input.c_str(), /*IsText=*/true);
  if (std::error_code ec = buffer.getError()) {
    return llvm::createStringError(ec, "Failed to open input file %s",
                                   input.c_str());
  }

  const MCTargetOptions options = mc::InitMCTargetOptionsFromFlags();
  std::unique_ptr<MCRegisterInfo> mcri(target->createMCRegInfo(TripleName));
  std::unique_ptr<MCAsmInfo> mcai(
      target->createMCAsmInfo(*mcri, TripleName, options));
  std::unique_ptr<MCSubtargetInfo> msti(
      target->createMCSubtargetInfo(TripleName, "", ""));
  std::unique_ptr<MCInstrInfo> mcii(target->createMCInstrInfo());

  MCContext context(triple, mcai.get(), mcri.get(), msti.get());
  std::unique_ptr<MCObjectFileInfo> mcofi(
      target->createMCObjectFileInfo(context, /*PIC=*/false));
  context.setObjectFileInfo(mcofi.get());

  std::string microbenchAsm = (*buffer)->getBuffer().str();

  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  auto llvmContext = std::make_unique<LLVMContext>();
  auto inlineAsm = mlTarget->createInlineAsmBuilder();

  auto runner = llvm_ml::createCPUBenchmarkRunner(target, TripleName, pinnedCPU,
                                                  NumMaxRuns);

  if (numRepeat == 0) {
    auto testModule = llvm_ml::createCPUTestHarness(
        *llvmContext, microbenchAsm, numNoiseRepeat, 0, *inlineAsm);

    if (!testModule) {
      return llvm::createStringError(std::errc::invalid_argument,
                                     "Failed to generate test harness");
    }

    llvm::Expected<int> suggested =
        runner->check(std::move(testModule), numNoiseRepeat);
    if (!suggested)
      return suggested.takeError();

    numRepeat = std::max(*suggested, 200);
  }

  auto module = llvm_ml::createCPUTestHarness(
      *llvmContext, microbenchAsm, numNoiseRepeat, numRepeat, *inlineAsm);

  if (!module) {
    return llvm::createStringError(std::errc::invalid_argument,
                                   "Failed to generate test harness");
  }

  auto err = runner->run(std::move(module), numNoiseRepeat, numRepeat);
  if (err)
    return err;

  llvm::ArrayRef<llvm_ml::BenchmarkResult> noiseResults =
      runner->getNoiseResults();
  llvm::ArrayRef<llvm_ml::BenchmarkResult> workloadResults =
      runner->getWorkloadResults();

  const auto minEltPred = [](const auto &lhs, const auto &rhs) {
    return lhs.numCycles < rhs.numCycles;
  };

  auto minNoise =
      std::min_element(noiseResults.begin(), noiseResults.end(), minEltPred);
  auto minWorkload = std::min_element(workloadResults.begin(),
                                      workloadResults.end(), minEltPred);

  llvm_ml::Measurement m = *minWorkload - *minNoise;

  if (ReadableJSON) {
    m.exportJSON(output, (*buffer)->getBuffer(), noiseResults, workloadResults);
  } else {
    m.exportBinary(output, (*buffer)->getBuffer(), noiseResults,
                   workloadResults);
  }

  return Error::success();
}

int runBatch(fs::path input, fs::path output, const Target *target) {
  using namespace indicators;

  indicators::show_console_cursor(false);
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

  llvm::ThreadPool pool;

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
  dispatchedTasks.reserve(PinnedCPUs.size());

  size_t cpuId = 0;

  std::unique_ptr<raw_fd_ostream> os;
  if (LogFile != "") {
    std::error_code ec;
    os = std::make_unique<raw_fd_ostream>(LogFile, ec);
  }

  BlockProgressBar bar{
      option::BarWidth{80}, option::ForegroundColor{Color::green},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::MaxProgress{files.size()}};

  for (const auto &inpFile : files) {
    std::string newFilename = std::string{inpFile.filename().stem()} + ".cbuf";
    fs::path outFile = output / fs::path{newFilename};
    auto future = pool.async(
        [outFile, pinnedCPU = PinnedCPUs[cpuId], inpFile, target, &bar]() {
          auto err = runSingleFile(inpFile, outFile, target, NumRepeat,
                                   NumRepeatNoise, pinnedCPU);
          bar.tick();
          return err;
        });

    dispatchedTasks.push_back(std::move(future));
    cpuId++;

    if (dispatchedTasks.size() == PinnedCPUs.size()) {
      for (auto &f : dispatchedTasks) {
        f.wait();

        // TODO(Alex): this is definitely a UB
        if (const_cast<llvm::Error &>(f.get()) && os)
          *os << f.get() << "\n";
      }

      dispatchedTasks.clear();
      dispatchedTasks.reserve(PinnedCPUs.size());
      cpuId = 0;
    }
  }

  indicators::show_console_cursor(true);

  return 0;
}

int main(int argc, char **argv) {
  InitLLVM x(argc, argv);

  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllTargets();
  InitializeAllAsmPrinters();

  cl::ParseCommandLineOptions(argc, argv, "benchmark ASM basic blocks\n");

  const Target *target = getTarget();

  if (!target) {
    errs() << "Failed to create target. You can specify triple manually like "
              "--triple=x86_64-unknown-unknown\n";
    return 1;
  }

  fs::path input{std::string{InputFilename}};

  if (fs::is_directory(input)) {
    fs::path output{std::string{OutputFilename}};
    if (!fs::is_directory(output)) {
      llvm::errs() << output.c_str() << " is not a directory\n";
      return 1;
    }

    return runBatch(input, output, target);
  } else {
    llvm::outs() << "Running in single mode\n";
    fs::path output{std::string{OutputFilename}};
    int pinnedCPU = PinnedCPUs[0];
    if (auto err = runSingleFile(input, output, target, NumRepeat,
                                 NumRepeatNoise, pinnedCPU)) {
      llvm::errs() << err << "\n";
      return 1;
    }
  }

  return 0;
}
