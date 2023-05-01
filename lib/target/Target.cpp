//===--- Target.hpp - Target-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "Target.hpp"

namespace llvm_ml {
std::unique_ptr<MLTarget> createMLTarget(const llvm::Triple &triple,
                                         llvm::MCInstrInfo *mcii) {
  if (triple.getArchName() == "x86_64") {
    return createX86MLTarget(mcii);
  }

  llvm_unreachable("Unsupported target");
}
} // namespace llvm_ml
