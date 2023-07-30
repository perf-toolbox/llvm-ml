//===--- Target.hpp - Target-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/Support/Error.h"
#include "llvm/TargetParser/Triple.h"

#include <memory>
#include <set>
#include <string>

namespace llvm {
class MCInst;
class MCRegisterInfo;
class MCAsmInfo;
class MCSubtargetInfo;
class MCInstrInfo;
class SourceMgr;
class MCContext;
class MCTargetOptions;
class Target;
class IRBuilderBase;
class Value;
} // namespace llvm

namespace llvm_ml {
class InlineAsmBuilder {
public:
  virtual void createSaveState(llvm::IRBuilderBase &builder) = 0;
  virtual void createRestoreState(llvm::IRBuilderBase &builder) = 0;
  virtual void createBranch(llvm::IRBuilderBase &builder,
                            llvm::StringRef label) = 0;
  virtual void createLabel(llvm::IRBuilderBase &builder,
                           llvm::StringRef labelName) = 0;

  virtual ~InlineAsmBuilder() = default;
};

class MLTarget {
public:
  virtual ~MLTarget() = default;

  virtual std::set<unsigned> getReadRegisters(const llvm::MCInst &) = 0;
  virtual std::set<unsigned> getWriteRegisters(const llvm::MCInst &) = 0;

  virtual bool isMemLoad(const llvm::MCInst &inst) = 0;
  virtual bool isMemStore(const llvm::MCInst &inst) = 0;
  virtual bool isBarrier(const llvm::MCInst &inst) = 0;
  virtual bool isVector(const llvm::MCInst &inst) = 0;
  virtual bool isAtomic(const llvm::MCInst &inst) = 0;
  virtual bool isCompute(const llvm::MCInst &inst) = 0;
  virtual bool isNop(const llvm::MCInst &inst) = 0;
  virtual bool isFloat(const llvm::MCInst &inst) = 0;
  virtual bool isLea(const llvm::MCInst &inst) = 0;
  virtual bool isPush(const llvm::MCInst &inst) = 0;
  virtual bool isPop(const llvm::MCInst &inst) = 0;
  virtual bool isMov(const llvm::MCInst &inst) = 0;
  virtual bool isSyscall(const llvm::MCInst &inst) = 0;
  virtual bool isVarLatency(const llvm::MCInst &inst) = 0;

  virtual std::unique_ptr<InlineAsmBuilder> createInlineAsmBuilder() = 0;
};

std::unique_ptr<MLTarget> createMLTarget(const llvm::Triple &triple,
                                         llvm::MCInstrInfo *mcii);

std::unique_ptr<MLTarget> createX86MLTarget(llvm::MCInstrInfo *mcii);

llvm::Expected<std::vector<llvm::MCInst>>
parseAssembly(llvm::SourceMgr &srcMgr, const llvm::MCInstrInfo &mcii,
              const llvm::MCRegisterInfo &mcri, const llvm::MCAsmInfo &mcai,
              const llvm::MCSubtargetInfo &msti, llvm::MCContext &context,
              const llvm::Target *target, const llvm::Triple &triple,
              const llvm::MCTargetOptions &options);
} // namespace llvm_ml
