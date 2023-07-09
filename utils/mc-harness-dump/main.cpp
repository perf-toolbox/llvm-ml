//===--- main.cpp - Dump LLVM IR for benchmark harness --------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/target/Target.hpp"
#include "tools/llvm-mc-bench/BenchmarkGenerator.hpp"

#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::init("-"));

static cl::opt<int> NumRepeat("num-repeat",
                              cl::desc("number of basic block repititions"),
                              cl::init(200));
static cl::opt<int> NumRepeatNoise(
    "num-repeat-noise",
    cl::desc("number of basic block repititions for noise measurement"),
    cl::init(10));

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

static mc::RegisterMCTargetOptionsFlags MOF;

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
    llvm::errs() << "Failed to open input file " << InputFilename << "\n";
    return 1;
  }

  std::string microbenchAsm = (*buffer)->getBuffer().str();

  std::unique_ptr<MCInstrInfo> mcii(target->createMCInstrInfo());
  auto mlTarget = llvm_ml::createMLTarget(triple, mcii.get());

  auto llvmContext = std::make_unique<LLVMContext>();
  auto inlineAsm = mlTarget->createInlineAsmBuilder();

  auto module = llvm_ml::createCPUTestHarness(
      *llvmContext, microbenchAsm, NumRepeatNoise, NumRepeat, *inlineAsm);

  if (!module) {
    llvm::errs() << "Failed to generate test harness\n";
    return 1;
  }

  module->print(llvm::outs(), nullptr);

  return 0;
}
