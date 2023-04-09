//===--- counters_linux.cpp - Benchmark Linux PMU conuters ----------------===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <asm/unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "counters.hpp"

struct ReadFormat {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
};

namespace llvm_ml {

struct CountersContext {
  void *buf;
  uint64_t cyclesId;
  int baseFd;

  CountersContext() { buf = new char[4096]; }

  ~CountersContext() { delete[] static_cast<char *>(buf); }
};

CountersContext *counters_init() {
  auto *ctx = new CountersContext();

  struct perf_event_attr pea;

  pea.type = PERF_TYPE_HARDWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_HW_CPU_CYCLES;
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  ctx->baseFd = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
  ioctl(ctx->baseFd, PERF_EVENT_IOC_ID, &ctx->cyclesId);

  return ctx;
}

void counters_free(CountersContext *ctx) { delete ctx; }

} // namespace llvm_ml

extern "C" {
void counters_start(llvm_ml::CountersContext *ctx) {
  ioctl(ctx->baseFd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(ctx->baseFd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

void counters_stop(llvm_ml::CountersContext *ctx) {
  ioctl(ctx->baseFd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  read(ctx->baseFd, ctx->buf, 4096);
}

size_t counters_cycles(llvm_ml::CountersContext *ctx) {
  // TODO figure out perf_event_open
  return __builtin_ia32_rdtsc();
  ReadFormat *rf = static_cast<ReadFormat *>(ctx->buf);

  for (uint64_t i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == ctx->cyclesId) {
      return rf->values[i].value;
    }
  }

  return -1;
}
}
