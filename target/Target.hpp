//===--- Target.hpp - Target-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

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
} // namespace llvm

namespace llvm_ml {
class MLTarget {
public:
  virtual ~MLTarget() = default;
  virtual std::string getBenchPrologue() = 0;
  virtual std::string getBenchEpilogue() = 0;

  virtual std::set<unsigned> getReadRegisters(const llvm::MCInst &) = 0;
  virtual std::set<unsigned> getWriteRegisters(const llvm::MCInst &) = 0;

  virtual bool isMemLoad(const llvm::MCInst &inst) = 0;
  virtual bool isMemStore(const llvm::MCInst &inst) = 0;
  virtual bool isBarrier(const llvm::MCInst &inst) = 0;
  virtual bool isVector(const llvm::MCInst &inst) = 0;
  virtual bool isAtomic(const llvm::MCInst &inst) = 0;
  virtual bool isCompute(const llvm::MCInst &inst) = 0;
};

std::unique_ptr<MLTarget> createMLTarget(const llvm::Triple &triple,
                                         llvm::MCInstrInfo *mcii);

std::unique_ptr<MLTarget> createX86MLTarget(llvm::MCInstrInfo *mcii);
} // namespace llvm_ml
