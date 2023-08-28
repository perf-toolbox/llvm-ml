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
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void flush() = 0;

  virtual ~CountersContext() = default;
};

class DummyCounters : public CountersContext {
public:
  DummyCounters(CountersCb cb) : mCB(cb) {}

  void start() override {}
  void stop() override {}

  void flush() override {
    llvm::SmallVector<CounterValue> counters;
    counters.push_back(CounterValue{.type = Counter::Cycles, .value = 10});
    counters.push_back(
        CounterValue{.type = Counter::Instructions, .value = 20});
    counters.push_back(CounterValue{.type = Counter::CacheMisses, .value = 0});
    counters.push_back(
        CounterValue{.type = Counter::ContextSwitches, .value = 0});

    mCB(counters);
  }

private:
  CountersCb mCB;
};

class PMUCountersContext : public CountersContext {
public:
  PMUCountersContext(CountersCb cb) : mCB(cb) {
    pmu::Builder builder;
    builder.add_counter(pmu::CounterKind::Cycles)
        .add_counter(pmu::CounterKind::Instructions)
        .add_counter(pmu::CacheCounter{.level = pmu::CacheLevelKind::L1D,
                                       .kind = pmu::CacheCounterKind::Miss,
                                       .op = pmu::CacheOpKind::Read})
        .add_counter("SW:context_switches");
    mCounters = builder.build();
  }

  void start() override { mCounters->start(); }

  void stop() override { mCounters->stop(); }

  void flush() override {
    llvm::SmallVector<CounterValue> counters;

    for (auto value : *mCounters) {
      if (value.name.starts_with("cycles")) {
        counters.push_back(
            CounterValue{.type = Counter::Cycles,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name.starts_with("instructions")) {
        counters.push_back(
            CounterValue{.type = Counter::Instructions,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name.starts_with("cache_")) {
        counters.push_back(
            CounterValue{.type = Counter::CacheMisses,
                         .value = static_cast<size_t>(value.value)});
      } else if (value.name.starts_with("SW:context_switches")) {
        counters.push_back(
            CounterValue{.type = Counter::ContextSwitches,
                         .value = static_cast<size_t>(value.value)});
      }
    }

    mCB(counters);
  }

private:
  CountersCb mCB;
  std::unique_ptr<pmu::Counters> mCounters;
};

void flushCounters(CountersContext *ctx) { ctx->flush(); }

std::shared_ptr<CountersContext> createCounters(CountersCb cb) {
  if (std::getenv("LLVM_ML_BENCH_MOCK") != nullptr)
    return std::make_shared<DummyCounters>(std::move(cb));

  return std::make_shared<PMUCountersContext>(std::move(cb));
}

} // namespace llvm_ml

extern "C" {
void counters_start(llvm_ml::CountersContext *ctx) { ctx->start(); }

void counters_stop(llvm_ml::CountersContext *ctx) { ctx->stop(); }
}
