//===--- llvm-mc-bench.cpp - Benchmark ASM basic blocks -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "benchmark.hpp"
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

inline constexpr float kNoiseFrac = 0.1f;

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

static void createSingleTestFunction(StringRef functionName,
                                     ArrayRef<StringRef> assembly,
                                     int numRepeat, Module &module,
                                     IRBuilderBase &builder,
                                     llvm_ml::InlineAsmBuilder &inlineAsm) {
  auto &context = module.getContext();

  auto retTy = Type::getVoidTy(context);
  auto ptrTy = PointerType::getUnqual(context);

  auto funcTy = FunctionType::get(retTy, {ptrTy, ptrTy}, false);

  auto funcCallee = module.getOrInsertFunction(functionName, funcTy);
  (void)funcCallee;

  auto func = module.getFunction(functionName);

  auto *entry = llvm::BasicBlock::Create(context, "entry", func);

  builder.SetInsertPoint(entry);

  auto countersFuncTy = FunctionType::get(retTy, {ptrTy}, false);

  std::string startName = ("workload_start_" + functionName).str();
  std::string endName = ("workload_end_" + functionName).str();

  auto startCallee =
      module.getOrInsertFunction("counters_start", countersFuncTy);
  builder.CreateCall(startCallee, {func->getArg(0)});

  inlineAsm.createSaveState(builder);

  inlineAsm.createBranch(builder, startName);
  inlineAsm.createLabel(builder, startName);

  auto voidFuncTy = llvm::FunctionType::get(builder.getVoidTy(), false);

  for (int i = 0; i < numRepeat; i++) {
    for (auto line : assembly) {
      llvm::StringRef trimmed = line.trim();
      if (trimmed.empty())
        continue;
      auto asmCallee = llvm::InlineAsm::get(
          voidFuncTy, trimmed, "~{dirflag},~{fpsr},~{flags}", true);
      builder.CreateCall(asmCallee);
    }
  }

  inlineAsm.createBranch(builder, endName);
  inlineAsm.createLabel(builder, endName);

  inlineAsm.createRestoreState(builder);
  auto stopCallee = module.getOrInsertFunction("counters_stop", countersFuncTy);
  builder.CreateCall(stopCallee, {func->getArg(0)});

  builder.CreateRetVoid();
}

static std::unique_ptr<Module>
createTestHarness(LLVMContext &context, StringRef basicBlock,
                  llvm_ml::InlineAsmBuilder &inlineAsm) {
  auto module = std::make_unique<Module>("test_harness", context);
  IRBuilder builder(context);

  SmallVector<StringRef> lines;
  basicBlock.split(lines, '\n');

  int noiseRepeat = static_cast<int>(kNoiseFrac * NumRepeat);
  createSingleTestFunction("baseline", lines, noiseRepeat, *module, builder,
                           inlineAsm);

  createSingleTestFunction("bench", lines, noiseRepeat + NumRepeat, *module,
                           builder, inlineAsm);

  return module;
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
  }

  auto llvmContext = std::make_unique<LLVMContext>();
  auto inlineAsm = mlTarget->createInlineAsmBuilder();

  auto module = createTestHarness(*llvmContext, microbenchAsm, *inlineAsm);

  if (!module) {
    llvm::errs() << "Failed to generate test harness\n";
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
  cantFail(dylib.define(orc::absoluteSymbols(orc::SymbolMap({
      {mangle("counters_start"), countersStartPtr},
      {mangle("counters_stop"), countersStopPtr},
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

  int noiseRepeat = static_cast<int>(kNoiseFrac * NumRepeat);
  auto noiseResults =
      llvm_ml::runBenchmark(noiseFunc, PinnedCPU, NumRuns, noiseRepeat);
  auto workloadResults = llvm_ml::runBenchmark(benchFunc, PinnedCPU, NumRuns,
                                               noiseRepeat + NumRepeat);

  const auto minEltPred = [](const auto &lhs, const auto &rhs) {
    return lhs.numCycles < rhs.numCycles;
  };

  auto minNoise =
      std::min_element(noiseResults.begin(), noiseResults.end(), minEltPred);
  auto minWorkload = std::min_element(workloadResults.begin(),
                                      workloadResults.end(), minEltPred);

  llvm_ml::Measurement m = *minWorkload - *minNoise;
  int noiseRep = static_cast<int>(kNoiseFrac * NumRepeat);
  m.noiseNumRuns = noiseRep;
  m.workloadNumRuns = NumRepeat + noiseRep;
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
