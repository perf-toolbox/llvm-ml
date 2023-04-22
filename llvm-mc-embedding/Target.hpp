//===--- Target.hpp - Target-specific utilities ---------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <set>

namespace llvm {
class MCInst;
class MCRegisterInfo;
class MCAsmInfo;
class MCSubtargetInfo;
class MCInstrInfo;
} // namespace llvm

namespace llvm_ml {
class Target {
public:
  virtual ~Target() = default;

  virtual std::set<unsigned> getReadRegisters(const llvm::MCInst &) = 0;
  virtual std::set<unsigned> getWriteRegisters(const llvm::MCInst &) = 0;

  virtual bool isMemLoad(const llvm::MCInst &inst) = 0;
  virtual bool isMemStore(const llvm::MCInst &inst) = 0;
  virtual bool isBarrier(const llvm::MCInst &inst) = 0;
  virtual bool isVector(const llvm::MCInst &inst) = 0;
  virtual bool isAtomic(const llvm::MCInst &inst) = 0;
  virtual bool isCompute(const llvm::MCInst &inst) = 0;
};

std::unique_ptr<Target> createX86Target(llvm::MCRegisterInfo *ri,
                                        llvm::MCAsmInfo *ai,
                                        llvm::MCSubtargetInfo *sti,
                                        llvm::MCInstrInfo *ii);
} // namespace llvm_ml
