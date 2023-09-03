//===--- counters.hpp - Benchmark PMU conuters interface  -------------C++-===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/ArrayRef.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace llvm_ml {
enum class Counter {
  Cycles,
  Instructions,
  MicroOps,
  ContextSwitches,
  CacheMisses,
  MisalignedLoads,
};

struct CounterValue {
  Counter type;
  size_t value;
};

using CountersCb = std::function<void(llvm::ArrayRef<CounterValue>)>;

class CountersContext;

void prefetchCounters(CountersContext *ctx);
void flushCounters(CountersContext *ctx);
std::shared_ptr<CountersContext> createCounters(CountersCb cb);
} // namespace llvm_ml

extern "C" {
void counters_start(llvm_ml::CountersContext *);
void counters_stop(llvm_ml::CountersContext *);
}
