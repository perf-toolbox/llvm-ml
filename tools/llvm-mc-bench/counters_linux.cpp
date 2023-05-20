//===--- counters_linux.cpp - Benchmark Linux PMU conuters ----------------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

#include <asm/unistd.h>
#include <errno.h>
#include <functional>
#include <inttypes.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <memory>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

#include "counters.hpp"

struct ReadFormat {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
};

namespace llvm_ml {
struct CounterFd {
  Counter type;
  uint64_t id;
  int fd;
};

inline constexpr size_t kBufSize = 32768;

class CountersContext {
public:
  CountersContext(const CountersCb &cb) : mCB(cb) {
    mBuf.resize(kBufSize);
    int result;

    int err = pfm_initialize();
    if (err != PFM_SUCCESS) {
      llvm::errs() << "Failed to initialize libpfm\n";
      std::terminate();
    }

    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(struct perf_event_attr));

    pea.type = PERF_TYPE_HARDWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_HW_CPU_CYCLES;
    pea.disabled = 1;
    pea.pinned = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.exclude_idle = 1;
    pea.exclude_callchain_kernel = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    CounterFd baseFd;
    baseFd.type = Counter::Cycles;
    baseFd.fd = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
    if (baseFd.fd == -1) {
      perror("Error opening leader");
      std::terminate();
    }
    result = ioctl(baseFd.fd, PERF_EVENT_IOC_ID, &baseFd.id);
    if (result == -1) {
      perror("Error opening leader");
      std::terminate();
    }
    mDescriptors.push_back(baseFd);

    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_HARDWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_HW_INSTRUCTIONS;
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.exclude_idle = 1;
    pea.exclude_callchain_kernel = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    CounterFd instrFd;
    instrFd.type = Counter::Instructions;
    instrFd.fd = syscall(__NR_perf_event_open, &pea, 0, -1, baseFd.fd, 0);
    if (instrFd.fd == -1) {
      llvm::errs() << "Error opening instructions fd\n";
      std::terminate();
    }
    result = ioctl(instrFd.fd, PERF_EVENT_IOC_ID, &instrFd.id);
    if (result == -1) {
      perror("Error opening cycles");
      std::terminate();
    }
    mDescriptors.push_back(instrFd);

    pfm_perf_encode_arg_t arg;
    memset(&arg, 0, sizeof(arg));
    arg.attr = &pea;
    arg.fstr = NULL;
    arg.size = sizeof(arg);

    if (pfm_get_os_event_encoding("UOPS_RETIRED:ALL", PFM_PLM0 | PFM_PLM3,
                                  PFM_OS_PERF_EVENT_EXT, &arg) == PFM_SUCCESS) {
      memset(&pea, 0, sizeof(struct perf_event_attr));
      pea.type = PERF_TYPE_RAW;
      pea.size = sizeof(struct perf_event_attr);
      pea.config = arg.idx;
      pea.sample_period = 0;
      pea.sample_type = PERF_SAMPLE_READ;
      pea.wakeup_events = 1;
      pea.disabled = 1;
      pea.exclude_kernel = 1;
      pea.exclude_hv = 1;
      pea.exclude_idle = 1;
      pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

      CounterFd uopsFd;
      uopsFd.type = Counter::MicroOps;
      uopsFd.fd = syscall(__NR_perf_event_open, &pea, 0, -1, baseFd.fd, 0);
      if (uopsFd.fd == -1) {
        llvm::errs() << "Error opening uops fd\n";
        std::terminate();
      }
      ioctl(uopsFd.fd, PERF_EVENT_IOC_ID, &uopsFd.id);
      mDescriptors.push_back(uopsFd);
    } else if (pfm_get_os_event_encoding("RETIRED_UOPS", PFM_PLM0 | PFM_PLM3,
                                         PFM_OS_PERF_EVENT_EXT,
                                         &arg) == PFM_SUCCESS) {
      memset(&pea, 0, sizeof(struct perf_event_attr));
      pea.type = PERF_TYPE_RAW;
      pea.size = sizeof(struct perf_event_attr);
      pea.config = arg.idx;
      pea.sample_period = 0;
      pea.sample_type = PERF_SAMPLE_READ;
      pea.wakeup_events = 1;
      pea.disabled = 1;
      pea.exclude_kernel = 1;
      pea.exclude_hv = 1;
      pea.exclude_idle = 1;
      pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

      CounterFd uopsFd;
      uopsFd.type = Counter::MicroOps;
      uopsFd.fd = syscall(__NR_perf_event_open, &pea, 0, -1, baseFd.fd, 0);
      if (uopsFd.fd == -1) {
        llvm::errs() << "Error opening uops fd\n";
        std::terminate();
      }
      result = ioctl(uopsFd.fd, PERF_EVENT_IOC_ID, &uopsFd.id);
      if (result == -1) {
        perror("Error opening uops");
        std::terminate();
      }
      mDescriptors.push_back(uopsFd);
    }

    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_HW_CACHE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                 (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    pea.disabled = 1;
    pea.exclude_kernel = 1;
    pea.exclude_hv = 1;
    pea.exclude_idle = 1;
    pea.exclude_callchain_kernel = 1;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    CounterFd cacheFd;
    cacheFd.type = Counter::CacheMisses;
    cacheFd.fd = syscall(__NR_perf_event_open, &pea, 0, -1, baseFd.fd, 0);
    if (cacheFd.fd == -1) {
      llvm::errs() << "Error opening cache misses fd\n";
      std::terminate();
    }
    result = ioctl(cacheFd.fd, PERF_EVENT_IOC_ID, &cacheFd.id);
    if (result == -1) {
      perror("Error opening cache misses");
      std::terminate();
    }
    mDescriptors.push_back(cacheFd);

    memset(&pea, 0, sizeof(struct perf_event_attr));
    pea.type = PERF_TYPE_SOFTWARE;
    pea.size = sizeof(struct perf_event_attr);
    pea.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
    pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
    CounterFd contextSwitchFd;
    contextSwitchFd.type = Counter::ContextSwitches;
    contextSwitchFd.fd =
        syscall(__NR_perf_event_open, &pea, 0, -1, baseFd.fd, 0);
    if (contextSwitchFd.fd == -1) {
      llvm::errs() << "Error opening context switches fd\n";
      std::terminate();
    }
    result = ioctl(contextSwitchFd.fd, PERF_EVENT_IOC_ID, &contextSwitchFd.id);
    if (result == -1) {
      perror("Error opening context switches");
      std::terminate();
    }
    mDescriptors.push_back(contextSwitchFd);
  }

  void start() {
    int result;
    result = ioctl(mDescriptors.front().fd, PERF_EVENT_IOC_RESET,
                   PERF_IOC_FLAG_GROUP);
    if (result == -1) {
      perror("Error resetting event");
      std::terminate();
    }
    result = ioctl(mDescriptors.front().fd, PERF_EVENT_IOC_ENABLE,
                   PERF_IOC_FLAG_GROUP);
    if (result == -1) {
      perror("Error enabling event");
      std::terminate();
    }
  }

  void stop() {
    int result;
    result = ioctl(mDescriptors.front().fd, PERF_EVENT_IOC_DISABLE,
                   PERF_IOC_FLAG_GROUP);
    if (result == -1) {
      perror("Error disabling event");
      std::terminate();
    }
    result = read(mDescriptors.front().fd, mBuf.data(), kBufSize);
    if (result == -1) {
      perror("Error reading from fd");
      std::terminate();
    }
  }

  void flush() {
    ReadFormat *rf = reinterpret_cast<ReadFormat *>(mBuf.data());

    llvm::SmallVector<CounterValue> counters;

    for (uint64_t i = 0; i < rf->nr; i++) {
      for (auto &fd : mDescriptors) {
        if (rf->values[i].id == fd.id) {
          counters.push_back(CounterValue{fd.type, rf->values[i].value});
          break;
        }
      }
    }

    mCB(counters);
  }

  ~CountersContext() {
    pfm_terminate();

    for (auto &fd : mDescriptors) {
      close(fd.fd);
    }
  }

private:
  CountersCb mCB;
  std::vector<CounterFd> mDescriptors;
  std::vector<char> mBuf;
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
