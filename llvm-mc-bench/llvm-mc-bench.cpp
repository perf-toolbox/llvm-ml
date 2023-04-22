//===--- llvm-mc-bench.cpp - Benchmark ASM basic blocks -------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

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

#include "benchmark.hpp"
#include "counters.hpp"

using namespace llvm;

constexpr auto kHarnessTemplate = R"(
      declare void @counters_start(ptr noundef)
      declare void @counters_stop(ptr noundef)
      declare i64 @counters_cycles(ptr noundef)

      define void @bench(ptr noundef %ctrctx, ptr noundef %out) {
      entry:
        call void @counters_start(ptr noundef %ctrctx)
        %before = call i64 @counters_cycles(ptr noundef %ctrctx)

        ; WORKLOAD
        call void @counters_stop(ptr noundef %ctrctx)

        %after = call i64 @counters_cycles(ptr noundef %ctrctx)

        %cycles = sub i64 %after, %before
        store i64 %cycles, ptr %out, align 8

        ret void
      }

      define void @baseline(ptr noundef %ctrctx, ptr noundef %out) {
      entry:
        call void @counters_start(ptr noundef %ctrctx)
        %before = call i64 @counters_cycles(ptr noundef %ctrctx)

        ; NOISE 

        call void @counters_stop(ptr noundef %ctrctx)

        %after = call i64 @counters_cycles(ptr noundef %ctrctx)

        %cycles = sub i64 %after, %before
        store i64 %cycles, ptr %out, align 8

        ret void
      }

      attributes #1 = { nounwind }
    )";

// TODO(Alex) I wonder if the same can be achieved by simply putting
// all of the assembly basic blocks into their own function. Would this
// make the code more portable and concise?
constexpr auto Prologue = R"(
  push %rax
  push %rbx
  push %rcx
  push %rdx
  push %rsi
  push %rdi
  push %r8
  push %r9
  push %r10
  push %r11
  push %r12
  push %r13
  push %r14
  push %r15

  movq %rbp, %rax
  movq $$0x2325000, %rbx
  movq %rax, (%rbx)

  movq %rsp, %rax
  movq %rax, 16(%rbx)

  movq $$512, %rdi
  movq $$0x2324000, %rbx
  shr $$12, %rbx
  shl $$12, %rbx

  movq %rax, %rbp
  add $$2048, %rbp
  mov %rbp, %rsp
  shr $$5, %rsp
  shl $$5, %rsp
  sub $$0x10, %rsp

  movq $$0x2324000, %rax 
  movq $$0x2324000, %rbx  
  movq $$0x2324000, %rcx 
  movq $$0x2324000, %rdx 
  movq $$0x2324000, %rsi 
  movq $$0x2324000, %rdi 
  movq $$0x2324000, %r8  
  movq $$0x2324000, %r9  
  movq $$0x2324000, %r10 
  movq $$0x2324000, %r11 
  movq $$0x2324000, %r12 
  movq $$0x2324000, %r13 
  movq $$0x2324000, %r14 
  movq $$0x2324000, %r15 
)";

constexpr auto Epilogue = R"(
  movq $$0x2325000, %rbx
  movq (%rbx), %rax
  movq %rax, %rbp

  movq 16(%rbx), %rax
  movq %rax, %rsp

  pop %r15
  pop %r14
  pop %r13
  pop %r12
  pop %r11
  pop %r10
  pop %r9
  pop %r8
  pop %rdi
  pop %rsi
  pop %rdx
  pop %rcx
  pop %rbx
  pop %rax
)";

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("output file"),
                                           cl::init("-"));
static cl::opt<int> NumRuns("n", cl::desc("number or repititions"),
                            cl::init(10000));

static cl::opt<int>
    PinnedCPU("c", cl::desc("id of the CPU core to pin this process to"),
              cl::init(0));

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

  const auto buildInlineAsm = [&](bool addWorkload) {
    std::string inlineAsm = "call void asm sideeffect \"";

    inlineAsm += Prologue;

    if (addWorkload) {
      for (int i = 0; i < NumRuns; i++) {
        inlineAsm += microbenchAsm;
      }
    }
    inlineAsm += Epilogue;

    inlineAsm += "\", \"\"() #1\n";

    return inlineAsm;
  };

  std::string completeHarness = kHarnessTemplate;
  {
    size_t insertPos = completeHarness.find("; WORKLOAD");
    completeHarness.insert(insertPos, buildInlineAsm(true));
    insertPos = completeHarness.find("; NOISE");
    completeHarness.insert(insertPos, buildInlineAsm(false));
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

  std::string result;
  raw_string_ostream ros{result};
  ros << "results:\n";
  ros << "  num_runs: " << NumRuns << "\n";

  size_t noise = 0;
  const auto noiseCb = [&noise, &ros](size_t cycles) {
    noise = cycles;
    ros << "  noise: " << cycles << "\n";
  };
  const auto benchCb = [&noise, &ros](size_t cycles) {
    ros << "  total_cycles: " << cycles << "\n";
    ros << "  cycles: " << (cycles - noise) << "\n";
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

  ros.flush();

  std::error_code errorCode;
  raw_fd_ostream outfile(OutputFilename, errorCode, sys::fs::OF_None);
  outfile << result;
  outfile.close();

  return 0;
}
