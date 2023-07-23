//===--- counters_linux.cpp - Benchmark Linux PMU conuters ----------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "pmu/pmu.hpp"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <memory>
#include <vector>

#include "counters.hpp"

namespace llvm_ml {
class CountersContext {
public:
  CountersContext(const CountersCb &cb) : mCB(cb) {
    pmu::Builder builder;
    builder.add_counter(pmu::CounterKind::Cycles)
        .add_counter(pmu::CounterKind::Instructions)
        .add_counter(pmu::CounterKind::CacheMisses)
        .add_counter("SW:context_switches");
    mCounters = builder.build();
  }

  void start() { mCounters->start(); }

  void stop() { mCounters->stop(); }

  void flush() {
    llvm::SmallVector<CounterValue> counters;

    for (auto value : *mCounters) {
      if (value.name == "cycles") {
        counters.push_back(
            CounterValue{.type = Counter::Cycles,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name == "instructions") {
        counters.push_back(
            CounterValue{.type = Counter::Instructions,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name == "cache_misses") {
        counters.push_back(
            CounterValue{.type = Counter::CacheMisses,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name == "SW:context_switches") {
        counters.push_back(
            CounterValue{.type = Counter::ContextSwitches,
                         .value = static_cast<size_t>(value.value)});
      }
    }

    mCB(counters);
  }

  ~CountersContext() = default;

private:
  CountersCb mCB;
  std::unique_ptr<pmu::Counters> mCounters;
};

void flushCounters(CountersContext *ctx) { ctx->flush(); }

std::shared_ptr<CountersContext> createCounters(const CountersCb &cb) {
  return std::make_shared<CountersContext>(cb);
}

} // namespace llvm_ml

extern "C" {
void counters_start(llvm_ml::CountersContext *ctx) { ctx->start(); }

void counters_stop(llvm_ml::CountersContext *ctx) { ctx->stop(); }
}
