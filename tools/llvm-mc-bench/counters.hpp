//===--- counters.hpp - Benchmark PMU conuters interface  -------------C++-===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>

namespace llvm_ml {
struct CountersContext;

CountersContext *counters_init();
void counters_free(CountersContext *);
uint64_t counters_context_switches(CountersContext *ctx);
uint64_t counters_cache_misses(CountersContext *ctx);
} // namespace llvm_ml

extern "C" {
void counters_start(llvm_ml::CountersContext *);
void counters_stop(llvm_ml::CountersContext *);
uint64_t counters_cycles(llvm_ml::CountersContext *);
}
