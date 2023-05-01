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

#include <iostream>

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
  uint64_t cacheMissId;
  uint64_t contextSwitchId;
  int baseFd;
  int cacheMissFd;
  int contextSwitchFd;

  CountersContext() { buf = new char[4096]; }

  ~CountersContext() { delete[] static_cast<char *>(buf); }
};

CountersContext *counters_init() {
  auto *ctx = new CountersContext();

  struct perf_event_attr pea;

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_HARDWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_HW_CPU_CYCLES;
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  ctx->baseFd = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
  if (ctx->baseFd == -1) {
    std::cerr << "Error opening leader\n";
    std::terminate();
  }
  ioctl(ctx->baseFd, PERF_EVENT_IOC_ID, &ctx->cyclesId);

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_HARDWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_HW_CACHE_MISSES;
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  ctx->cacheMissFd = syscall(__NR_perf_event_open, &pea, 0, -1, ctx->baseFd, 0);
  if (ctx->cacheMissFd == -1) {
    std::cerr << "Error opening cache misses fd\n";
    std::terminate();
  }
  ioctl(ctx->cacheMissFd, PERF_EVENT_IOC_ID, &ctx->cacheMissId);

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_SOFTWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  ctx->contextSwitchFd =
      syscall(__NR_perf_event_open, &pea, 0, -1, ctx->baseFd, 0);
  if (ctx->contextSwitchFd == -1) {
    std::cerr << "Error opening context switches fd\n";
    std::terminate();
  }
  ioctl(ctx->contextSwitchFd, PERF_EVENT_IOC_ID, &ctx->contextSwitchId);

  return ctx;
}

void counters_free(CountersContext *ctx) { delete ctx; }

uint64_t counters_context_switches(llvm_ml::CountersContext *ctx) {
  ReadFormat *rf = static_cast<ReadFormat *>(ctx->buf);

  for (uint64_t i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == ctx->contextSwitchId) {
      return rf->values[i].value;
    }
  }

  return -1;
}
uint64_t counters_cache_misses(llvm_ml::CountersContext *ctx) {
  ReadFormat *rf = static_cast<ReadFormat *>(ctx->buf);

  for (uint64_t i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == ctx->cacheMissId) {
      return rf->values[i].value;
    }
  }

  return -1;
}
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

uint64_t counters_cycles(llvm_ml::CountersContext *ctx) {
  ReadFormat *rf = static_cast<ReadFormat *>(ctx->buf);

  for (uint64_t i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == ctx->cyclesId) {
      return rf->values[i].value;
    }
  }

  return -1;
}
}
