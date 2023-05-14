//===--- llvm-mc-bench.cpp - Benchmark ASM basic blocks -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "benchmark.hpp"
#include "counters.hpp"
#include "lib/structures/bb_metrics.pb.h"
#include "llvm-ml/target/Target.hpp"

#include "llvm/AsmParser/Parser.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
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

#include <nlohmann/json.hpp>

using namespace llvm;
using json = nlohmann::json;

constexpr auto kHarnessTemplate = R"(
      declare void @counters_start(ptr noundef)
      declare void @counters_stop(ptr noundef)
      declare i64 @counters_cycles(ptr noundef)

      define void @bench(ptr noundef %ctrctx, ptr noundef %out) {
      entry:
        call void @counters_start(ptr noundef %ctrctx)

        ; WORKLOAD
        call void @counters_stop(ptr noundef %ctrctx)

        %cycles = call i64 @counters_cycles(ptr noundef %ctrctx)

        store i64 %cycles, ptr %out, align 8

        ret void
      }

      define void @baseline(ptr noundef %ctrctx, ptr noundef %out) {
      entry:
        call void @counters_start(ptr noundef %ctrctx)

        ; NOISE 

        call void @counters_stop(ptr noundef %ctrctx)

        %cycles = call i64 @counters_cycles(ptr noundef %ctrctx)

        store i64 %cycles, ptr %out, align 8

        ret void
      }

      attributes #1 = { nounwind }
    )";

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("output file"),
                                           cl::init("-"));
static cl::opt<int> NumRuns("n", cl::desc("number or repititions"),
                            cl::init(200));

static cl::opt<int>
    PinnedCPU("c", cl::desc("id of the CPU core to pin this process to"),
              cl::init(0));

static cl::opt<bool>
    ReadableJSON("readable-json",
                 cl::desc("export measurements to a JSON file"), cl::init(0));
static cl::opt<bool>
    IncludeSource("include-source",
                  cl::desc("add source basic block to results file"),
                  cl::init(0));

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to assemble for, "
                              "see -version for available targets"));

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple to assemble for, "
                                  "see -version for available targets"));

struct Measurement {
  uint64_t cycles;
  uint64_t cacheMisses;
  uint64_t contextSwitches;
  size_t numRuns;
};

static void exportJSON(const Measurement &noise, const Measurement &workload,
                       const std::string &source, llvm::raw_ostream &os) {
  json res;
  res["noise_cycles"] = noise.cycles;
  res["noise_cache_misses"] = noise.cacheMisses;
  res["noise_context_switches"] = noise.contextSwitches;
  res["total_cycles"] = workload.cycles;
  res["total_cache_misses"] = workload.cacheMisses;
  res["total_context_switches"] = workload.contextSwitches;
  if (noise.cycles < workload.cycles) {
    res["measured_cycles"] = workload.cycles - noise.cycles;
  } else {
    res["measured_cycles"] = 0;
  }
  res["num_runs"] = workload.numRuns - noise.numRuns;
  if (IncludeSource) {
    res["source"] = source;
  }

  os << res.dump(4);
}

static void exportPBuf(const Measurement &noise, const Measurement &workload,
                       const std::string &source, llvm::raw_ostream &os) {
  llvm_ml::BBMetrics metrics;
  metrics.set_noise_cycles(noise.cycles);
  metrics.set_noise_cache_misses(noise.cacheMisses);
  metrics.set_noise_context_switches(noise.contextSwitches);
  metrics.set_total_cycles(workload.cycles);
  metrics.set_total_cache_misses(workload.cacheMisses);
  metrics.set_total_context_switches(workload.contextSwitches);
  metrics.set_num_runs(workload.numRuns - noise.numRuns);
  if (noise.cycles < workload.cycles) {
    metrics.set_measured_cycles(workload.cycles - noise.cycles);
  } else {
    metrics.set_measured_cycles(0);
  }
  if (IncludeSource) {
    metrics.set_source(source);
  }

  std::string serialized;
  metrics.SerializeToString(&serialized);
  os << serialized;
}

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

  // Prepare asm string
  {
    size_t pos = 0;

    while (pos = microbenchAsm.find("$", pos), pos != std::string::npos) {
      microbenchAsm.insert(pos, "$");
      pos += 2;
    }
    pos = 0;
    while (pos = microbenchAsm.find("\n", pos), pos != std::string::npos) {
      microbenchAsm = microbenchAsm.replace(pos, 1, "\\0A");
      pos += 3;
    }
  }

  const auto buildInlineAsm = [&](size_t numRep) {
    std::string inlineAsm = "call void asm sideeffect \"";

    inlineAsm += mlTarget->getBenchPrologue();

    for (size_t i = 0; i < numRep; i++) {
      inlineAsm += microbenchAsm;
    }
    inlineAsm += mlTarget->getBenchEpilogue();

    inlineAsm += "\", \"\"() #1\n";

    return inlineAsm;
  };

  size_t noiseRep = static_cast<size_t>(0.1 * NumRuns);
  std::string completeHarness = kHarnessTemplate;
  {
    size_t insertPos = completeHarness.find("; WORKLOAD");
    completeHarness.insert(insertPos, buildInlineAsm(noiseRep));
    insertPos = completeHarness.find("; NOISE");
    completeHarness.insert(insertPos, buildInlineAsm(NumRuns + noiseRep));
  }

  auto llvmContext = std::make_unique<LLVMContext>();

  SMDiagnostic error;
  std::unique_ptr<Module> module =
      parseAssemblyString(completeHarness, error, *llvmContext);
  if (!module) {
    errs() << "Error: " << error.getMessage() << "\n";
    return 1;
  }

  auto JTMB = orc::JITTargetMachineBuilder::detectHost();

  if (!JTMB)
    return 1;

  auto DL = JTMB->getDefaultDataLayoutForTarget();
  if (!DL)
    return 1;

  auto jit = cantFail(orc::LLJITBuilder().create());

  orc::MangleAndInterner mangle(jit->getExecutionSession(), *DL);

  auto &dylib = jit->getMainJITDylib();
  orc::ExecutorSymbolDef countersStartPtr(
      orc::ExecutorAddr::fromPtr(&counters_start), JITSymbolFlags::Exported);
  orc::ExecutorSymbolDef countersStopPtr(
      orc::ExecutorAddr::fromPtr(&counters_stop), JITSymbolFlags::Exported);
  orc::ExecutorSymbolDef countersCyclesPtr(
      orc::ExecutorAddr::fromPtr(&counters_cycles), JITSymbolFlags::Exported);
  cantFail(dylib.define(orc::absoluteSymbols(orc::SymbolMap({
      {mangle("counters_start"), countersStartPtr},
      {mangle("counters_stop"), countersStopPtr},
      {mangle("counters_cycles"), countersCyclesPtr},
  }))));

  cantFail(jit->addIRModule(
      orc::ThreadSafeModule(std::move(module), std::move(llvmContext))));

  auto benchSymbol = jit->lookup("bench");
  if (!benchSymbol) {
    errs() << "Error: " << toString(benchSymbol.takeError()) << "\n";
    return 1;
  }
  auto noiseSymbol = jit->lookup("baseline");
  if (!noiseSymbol) {
    errs() << "Error: " << toString(noiseSymbol.takeError()) << "\n";
    return 1;
  }

  llvm_ml::BenchmarkFn benchFunc = benchSymbol->toPtr<llvm_ml::BenchmarkFn>();
  llvm_ml::BenchmarkFn noiseFunc = noiseSymbol->toPtr<llvm_ml::BenchmarkFn>();

  Measurement noise;
  const auto noiseCb = [&noise, noiseRep](uint64_t cycles, uint64_t cacheMisses,
                                          uint64_t contextSwitches) {
    noise.cycles = cycles;
    noise.contextSwitches = contextSwitches;
    noise.cacheMisses = cacheMisses;
    noise.numRuns = noiseRep;
  };

  Measurement workload;
  const auto benchCb = [&workload, noiseRep](uint64_t cycles,
                                             uint64_t cacheMisses,
                                             uint64_t contextSwitches) {
    workload.cycles = cycles;
    workload.contextSwitches = contextSwitches;
    workload.cacheMisses = cacheMisses;
    workload.numRuns = noiseRep + NumRuns;
  };

  auto maybeErr = llvm_ml::runBenchmark(noiseFunc, noiseCb, PinnedCPU);
  if (maybeErr) {
    errs() << "Failed to measure system noise... " << maybeErr << "\n";
    return 1;
  }
  maybeErr = llvm_ml::runBenchmark(benchFunc, benchCb, PinnedCPU);
  if (maybeErr) {
    errs() << "Child terminated abnormally... " << maybeErr << "\n";
    return 1;
  }

  std::error_code errorCode;
  raw_fd_ostream outfile(OutputFilename, errorCode, sys::fs::OF_None);
  if (ReadableJSON) {
    exportJSON(noise, workload, (*buffer)->getBuffer().str(), outfile);
  } else {
    exportPBuf(noise, workload, (*buffer)->getBuffer().str(), outfile);
  }
  outfile.close();

  return 0;
}
