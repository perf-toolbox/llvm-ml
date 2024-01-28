//===--- llvm-mc-embedding.cpp - Generate tokens in CSV format ------------===//
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
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/Threading.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TargetParser/Host.h"

namespace fs = std::filesystem;

static llvm::mc::RegisterMCTargetOptionsFlags MOF;

static llvm::cl::opt<std::string> OutputFilename("o",
                                                 llvm::cl::desc("output file"));

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

  const llvm::MCTargetOptions options =
      llvm::mc::InitMCTargetOptionsFromFlags();
  std::unique_ptr<llvm::MCRegisterInfo> mcri(
      target->createMCRegInfo(TripleName));
  std::unique_ptr<llvm::MCAsmInfo> mcai(
      target->createMCAsmInfo(*mcri, TripleName, options));
  std::unique_ptr<llvm::MCSubtargetInfo> msti(
      target->createMCSubtargetInfo(TripleName, "", ""));
  std::unique_ptr<llvm::MCInstrInfo> mcii(target->createMCInstrInfo());

  std::error_code ec;
  llvm::raw_fd_ostream os{OutputFilename, ec};

  for (unsigned i = 0; i < mcii->getNumOpcodes(); i++) {
    os << i << ",\"" << mcii->getName(i) << "\"\n";
  }

  return 0;
}