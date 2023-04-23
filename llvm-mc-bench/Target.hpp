//===--- Target.hpp - Target-specific utils -------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <string>

namespace llvm_ml {
class Target {
public:
  virtual ~Target() = default;
  virtual std::string getPrologue() = 0;
  virtual std::string getEpilogue() = 0;
};

std::unique_ptr<Target> createX86Target();
} // namespace llvm_ml
