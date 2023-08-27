//===--- Target.hpp - Target-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "Target.hpp"

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
#include "llvm/Support/SourceMgr.h"

namespace llvm_ml {
std::unique_ptr<MLTarget> createMLTarget(const llvm::Triple &triple,
                                         llvm::MCInstrInfo *mcii) {
  if (triple.getArchName() == "x86_64") {
    return createX86MLTarget(mcii);
  }

  llvm_unreachable("Unsupported target");
}

namespace {
class MCStreamerWrapper final : public llvm::MCStreamer {
  std::vector<llvm::MCInst> &instrs;

public:
  MCStreamerWrapper(llvm::MCContext &Context, std::vector<llvm::MCInst> &instrs)
      : MCStreamer(Context), instrs(instrs) {}

  // We only want to intercept the emission of new instructions.
  void emitInstruction(const llvm::MCInst &inst,
                       const llvm::MCSubtargetInfo & /* unused */) override {
    instrs.push_back(inst);
  }

  bool emitSymbolAttribute(llvm::MCSymbol *, llvm::MCSymbolAttr) override {
    return true;
  }

  void emitCommonSymbol(llvm::MCSymbol *, uint64_t /*size*/,
                        llvm::Align) override {}
  void emitZerofill(llvm::MCSection *, llvm::MCSymbol *symbol = nullptr,
                    uint64_t size = 0,
                    llvm::Align byteAlignment = llvm::Align(1),
                    llvm::SMLoc loc = llvm::SMLoc()) override {}
  void emitGPRel32Value(const llvm::MCExpr *) override {}
  void beginCOFFSymbolDef(const llvm::MCSymbol *) override {}
  void emitCOFFSymbolStorageClass(int /*storageClass*/) override {}
  void emitCOFFSymbolType(int /*type*/) override {}
  void endCOFFSymbolDef() override {}
};
} // namespace

llvm::Expected<std::vector<llvm::MCInst>>
parseAssembly(llvm::SourceMgr &srcMgr, const llvm::MCInstrInfo &mcii,
              const llvm::MCRegisterInfo &mcri, const llvm::MCAsmInfo &mcai,
              const llvm::MCSubtargetInfo &msti, llvm::MCContext &context,
              const llvm::Target *target, const llvm::Triple &triple,
              const llvm::MCTargetOptions &options) {

  std::vector<llvm::MCInst> instructions;

  MCStreamerWrapper streamer(context, instructions);

  auto parser = llvm::createMCAsmParser(srcMgr, context, streamer, mcai);

  std::unique_ptr<llvm::MCTargetAsmParser> target_parser(
      target->createMCAsmParser(msti, *parser, mcii, options));

  if (!target_parser)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to create target parser");

  parser->setTargetParser(*target_parser);

  int parse_result = parser->Run(false);
  if (parse_result)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to parse assembly");

  return instructions;
}
} // namespace llvm_ml
