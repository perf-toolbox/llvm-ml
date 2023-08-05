//===--- structures.hpp - Data structures used by llvm-mc-* tools ---------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm-ml/structures/mc_dataset.capnp.h"
#include "llvm-ml/structures/mc_graph.capnp.h"
#include "llvm-ml/structures/mc_metrics.capnp.h"

#include "capnp/message.h"

#include <filesystem>
#include <functional>

namespace llvm_ml {
void writeToFile(std::filesystem::path path, capnp::MessageBuilder &builder);

template <typename T>
int readFromFile(std::filesystem::path path,
                 std::function<void(typename T::Reader &)>);
} // namespace llvm_ml
