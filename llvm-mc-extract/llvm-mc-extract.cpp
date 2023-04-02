//===--- llvm-mc-extract.cpp - Extract basic blocks from binary -----------===//
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
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input file>"));

static cl::opt<std::string> OutputDirectory("o", cl::desc("output directory"),
                                            cl::Required);

static cl::opt<std::string>
    ArchName("arch", cl::desc("Target arch to assemble for, "
                              "see -version for available targets"));

static cl::opt<std::string>
    TripleName("triple", cl::desc("Target triple to assemble for, "
                                  "see -version for available targets"));

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

  const auto isBlockTerminator = [&](const MCInst &inst) {
    const MCInstrDesc &desc = mcii->get(inst.getOpcode());
    return desc.isBranch() || desc.isReturn();
  };

  uint64_t blockCounter = 0;

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
                        (Twine(blockCounter) + ".s").str());
      return std::string(path.begin(), path.end());
    };

    std::error_code EC;
    auto os = std::make_unique<raw_fd_ostream>(getPath(), EC);
    if (EC) {
      errs() << "Failed to create a file: " << EC.message();
      std::terminate();
    }

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
        } else {
          instPrinter->printInst(&inst, sectionAddress + index, "", *msti, *os);
          *os << "\n";
        }

        index += instSize;
      } else {
        errs() << "Failed to parse block\n";
        break;
      }
    }

    blockCounter++;
    os->close();
  }
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  InitializeAllTargetInfos();
  InitializeAllTargetMCs();
  InitializeAllDisassemblers();

  cl::ParseCommandLineOptions(argc, argv,
                              "extract asm basic blocks from binary\n");

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

  return 0;
}
