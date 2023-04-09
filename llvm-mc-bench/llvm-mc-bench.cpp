//===--- llvm-mc-bench.cpp - Benchmark ASM basic blocks -------------------===//
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

      define i64 @bench(ptr noundef %0) {
      entry:
        call void @counters_start(ptr noundef %0)
        %before = call i64 @counters_cycles(ptr noundef %0)
        ; INSERT BASIC BLOCK HERE
        call void @counters_stop(ptr noundef %0)

        %after = call i64 @counters_cycles(ptr noundef %0)

        %1 = sub i64 %after, %before

        ret i64 %1 
      }

      attributes #1 = { nounwind }
    )";

constexpr auto Prologue = R"(
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

static mc::RegisterMCTargetOptionsFlags MOF;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<std::string> OutputFilename("o", cl::desc("output file"),
                                           cl::init("-"));
static cl::opt<int> NumRuns("n", cl::desc("number or repititions"),
                            cl::init(10000));

static llvm::cl::opt<std::string>
    ArchName("arch", llvm::cl::desc("Target arch to assemble for, "
                                    "see -version for available targets"));

static llvm::cl::opt<std::string>
    TripleName("triple", llvm::cl::desc("Target triple to assemble for, "
                                        "see -version for available targets"));

static const llvm::Target *getTarget() {
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

int main(int argc, char **argv) {
  InitLLVM x(argc, argv);

  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllTargets();
  InitializeAllAsmPrinters();

  cl::ParseCommandLineOptions(argc, argv, "benchmark ASM basic blocks\n");

  const llvm::Target *target = getTarget();

  if (!target) {
    errs() << "Failed to create target. You can specify triple manually like "
              "--triple=x86_64-unknown-unknown\n";
    return 1;
  }

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

  llvm::MCContext context(triple, mcai.get(), mcri.get(), msti.get());
  std::unique_ptr<llvm::MCObjectFileInfo> mcofi(
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

  std::string inlineAsm = "call void asm sideeffect \"";

  inlineAsm += Prologue;

  for (int i = 0; i < NumRuns; i++) {
    inlineAsm += microbenchAsm;
  }

  inlineAsm += "\", \"\"() #1\n";

  std::string completeHarness = kHarnessTemplate;
  size_t insertPos = completeHarness.find("; INSERT BASIC BLOCK HERE");
  completeHarness.insert(insertPos, inlineAsm);

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

  orc::MangleAndInterner Mangle(jit->getExecutionSession(), *DL);

  auto &dylib = jit->getMainJITDylib();
  orc::ExecutorSymbolDef countersStartPtr(
      orc::ExecutorAddr::fromPtr(&counters_start), JITSymbolFlags::Exported);
  orc::ExecutorSymbolDef countersStopPtr(
      orc::ExecutorAddr::fromPtr(&counters_stop), JITSymbolFlags::Exported);
  orc::ExecutorSymbolDef countersCyclesPtr(
      orc::ExecutorAddr::fromPtr(&counters_cycles), JITSymbolFlags::Exported);
  cantFail(dylib.define(llvm::orc::absoluteSymbols(orc::SymbolMap({
      {Mangle("counters_start"), countersStartPtr},
      {Mangle("counters_stop"), countersStopPtr},
      {Mangle("counters_cycles"), countersCyclesPtr},
  }))));

  /*
  auto dlsg =
  orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL->getGlobalPrefix());
  if (!dlsg) {
    llvm::errs() << "Failed to create library search generator\n";
    return 1;
  }
  jit->getMainJITDylib().addGenerator(std::move(*dlsg));
  */

  cantFail(jit->addIRModule(
      orc::ThreadSafeModule(std::move(module), std::move(llvmContext))));

  auto symbol = jit->lookup("bench");
  if (!symbol) {
    errs() << "Error: " << toString(symbol.takeError()) << "\n";
    return 1;
  }

  llvm_ml::BenchmarkFn benchFunc = symbol->toPtr<llvm_ml::BenchmarkFn>();

  auto maybeErr = llvm_ml::run_benchmark(benchFunc, "");
  if (maybeErr) {
    llvm::errs() << "Child terminated abnormally... " << maybeErr << "\n";
    return 1;
  }

  return 0;
}
