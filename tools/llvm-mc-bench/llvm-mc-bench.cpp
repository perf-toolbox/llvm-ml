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
#include "llvm/TargetParser/Host.h"

using namespace llvm;

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("output file"),
                                           cl::init("-"));
static cl::opt<int> NumRepeat("n",
                              cl::desc("number of basic block repititions"),
                              cl::init(200));
static cl::opt<int> NumRuns("r",
                            cl::desc("maximum number of test harness re-runs"),
                            cl::init(10));

static cl::opt<int>
    PinnedCPU("c", cl::desc("id of the CPU core to pin this process to"),
              cl::init(0));

static cl::opt<bool>
    ReadableJSON("readable-json",
                 cl::desc("export measurements to a JSON file"), cl::init(0));

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to assemble for, "
                              "see -version for available targets"));

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple to assemble for, "
                                  "see -version for available targets"));

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

int main(int argc, char **argv) {
  llvm::ExitOnError exitOnErr;

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

  Triple triple(TripleName);

  ErrorOr<std::unique_ptr<MemoryBuffer>> buffer =
      MemoryBuffer::getFileOrSTDIN(InputFilename, /*IsText=*/true);
  if (std::error_code EC = buffer.getError()) {
    llvm::errs() << "Failed to open input file " << InputFilename << "\n";
    return 1;
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

  auto module = llvm_ml::createCPUTestHarness(*llvmContext, microbenchAsm,
                                              NumRepeat, *inlineAsm);

  if (!module) {
    llvm::errs() << "Failed to generate test harness\n";
    return 1;
  }

  size_t noiseRepeat = static_cast<size_t>(llvm_ml::kNoiseFrac * NumRepeat);

  auto runner = llvm_ml::createCPUBenchmarkRunner(
      target, TripleName, std::move(module), noiseRepeat,
      noiseRepeat + NumRepeat, NumRuns);
  exitOnErr(runner->run());

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
  m.noiseNumRuns = noiseRepeat;
  m.workloadNumRuns = NumRepeat + noiseRepeat;
  m.measuredNumRuns = NumRepeat;

  std::error_code errorCode;
  raw_fd_ostream outfile(OutputFilename, errorCode, sys::fs::OF_None);
  if (ReadableJSON) {
    m.exportJSON(outfile, (*buffer)->getBuffer(), noiseResults,
                 workloadResults);
  } else {
    m.exportProtobuf(outfile, (*buffer)->getBuffer());
  }
  outfile.close();

  return 0;
}
