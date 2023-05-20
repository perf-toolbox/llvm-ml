//===--- benchmark_linux.cpp - Benchmark runner -----------------------C++-===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "llvm/Support/Error.h"

#include "benchmark.hpp"
#include "counters.hpp"

constexpr unsigned MAX_FAULTS = 30;

struct PageFaultCommand {
  void *addr;
  // TODO(Alex): figure out if we really need this
  bool unmapFirst;
};

// Global vairables are used to simplify the assembly code.
// These shall only be used by the child process.

/// File descriptor of shared memory
int gSharedMem = 42;
/// Function pointer of benchmark workload
llvm_ml::BenchmarkFn gBenchFn = nullptr;
/// Context to manipulate PMUs
llvm_ml::CountersContext *gCont;
/// Info about which address should be used to map page
PageFaultCommand *gCmd;
/// Pointer to PMU values to be passed to parent
void *gOut;

/// Aligns pointer to memory page size
static void alignPtr(size_t &ptr) {
  // FIXME: Assuming page size is 4096
  ptr >>= 12;
  ptr <<= 12;
}

static void alignPtr(void *&ptr) {
  size_t iptr = reinterpret_cast<size_t>(ptr);
  alignPtr(iptr);
  ptr = reinterpret_cast<void *>(iptr);
}

extern "C" {
void map_and_restart() {
  alignPtr(gCmd->addr);

  if (!gCmd->addr) {
    llvm::errs() << "Invalid access. First page is reserved by the OS.\n";
    raise(SIGABRT);
  }

  void *res = nullptr;
  // Special case for storing stack info
  if (gCmd->addr == (void *)0x2325000) {
    res = mmap(gCmd->addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_FIXED, gSharedMem, 4096);
  } else {
    res = mmap(gCmd->addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_FIXED, gSharedMem, 0);
  }

  if (!res) {
    llvm::errs() << "Failed to allocate pointer at address: " << gCmd->addr
                 << "\n";
    raise(SIGABRT);
  }

  gBenchFn(gCont, gOut);
  llvm_ml::flushCounters(gCont);
  close(gSharedMem);
  _exit(0);
}
void restart_only() {
  gBenchFn(gCont, gOut);
  llvm_ml::flushCounters(gCont);
  close(gSharedMem);
  _exit(0);
}
}

static void restartChild(pid_t pid, void *restart_addr) {
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, NULL, &regs);
  regs.rip = (unsigned long)restart_addr;
  ptrace(PTRACE_SETREGS, pid, NULL, &regs);
  ptrace(PTRACE_CONT, pid, NULL, NULL);
}

static int allocateSharedMemory() {
  constexpr auto path = "shmem-path";
  int fd = shm_open(path, O_RDWR | O_CREAT, 0777);
  shm_unlink(path);
  ftruncate(fd, 4 * PAGE_SIZE);
  return fd;
}

namespace llvm_ml::detail {
llvm::Expected<BenchmarkResult> runSingleBenchmark(BenchmarkFn bench,
                                                   int pinnedCPU) {
  int status;
  int shmemFD = allocateSharedMemory();

  pid_t child = fork();

  if (child == 0) {
    // FIXME(Alex): there must be a better way to wait for parent to become
    // ready
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Pin process to thread.
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(pinnedCPU, &cpuSet);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet) < 0) {
      llvm::errs() << "Failed to pin process to CPU #" << pinnedCPU << "\n";
      exit(1);
    }
    struct sched_param schedParam = {.sched_priority = 90};
    if (sched_setscheduler(0, SCHED_FIFO, &schedParam) < 0) {
      llvm::errs() << "WARNING: failed to update scheduler policy\n";
    }

    gBenchFn = bench;
    gSharedMem = shmemFD;

    gOut = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, shmemFD,
                4096 * 2);
    memset(gOut, 0, 4096);
    gCmd = (PageFaultCommand *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, shmemFD, 4096 * 3);

    // LLVM installs its own segfault handler. We don't need that.
    signal(SIGSEGV, SIG_DFL);

    auto counters = createCounters([](llvm::ArrayRef<CounterValue> values) {
      uint64_t *out = static_cast<uint64_t *>(gOut);
      for (const auto &val : values) {
        if (val.type == Counter::Cycles) {
          out[0] = val.value;
        } else if (val.type == Counter::CacheMisses) {
          out[1] = val.value;
        } else if (val.type == Counter::ContextSwitches) {
          out[2] = val.value;
        } else if (val.type == Counter::Instructions) {
          out[3] = val.value;
        } else if (val.type == Counter::MicroOps) {
          out[4] = val.value;
        }
      }
    });

    gCont = counters.get();

    bench(gCont, gOut);

    close(shmemFD);
    _exit(0);
  } else { // close(fds[0]);
    signal(SIGSEGV, SIG_DFL);

    gCmd = (PageFaultCommand *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, shmemFD, 4096 * 3);

    ptrace(PTRACE_SEIZE, child, NULL, NULL);

    void *lastSignaledInstruction = nullptr;
    bool mappingDone = false;

    for (unsigned i = 0; i < MAX_FAULTS; i++) {
      child = wait(&status);

      if (WIFEXITED(status) || child == -1) {
        if (WEXITSTATUS(status) == 0) {
          void *outPtr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                              shmemFD, 4096 * 2);
          uint64_t *ptr = static_cast<uint64_t *>(outPtr);
          uint64_t cycles = ptr[0];
          uint64_t cacheMisses = ptr[1];
          uint64_t contextSwitches = ptr[2];
          uint64_t instructions = ptr[3];
          uint64_t uops = ptr[4];

          // Try to eliminate as much noise as possible
          if (cacheMisses != 0 && (i + 1 != MAX_FAULTS)) {
            void *restartAddress = (void *)&restart_only;
            restartChild(child, restartAddress);

            continue;
          }
          if (contextSwitches != 0 && (i + 1 != MAX_FAULTS)) {
            void *restartAddress = (void *)&restart_only;
            restartChild(child, restartAddress);

            continue;
          }

          BenchmarkResult res;
          res.numCycles = cycles;
          res.numContextSwitches = contextSwitches;
          res.numCacheMisses = cacheMisses;
          res.numInstructions = instructions;
          res.numMicroOps = uops;
          return res;
        }

        return llvm::createStringError(
            std::make_error_code(std::errc::invalid_argument),
            "Process exited with error");
      }

      if (mappingDone) {
        return llvm::createStringError(
            std::make_error_code(std::errc::invalid_argument),
            "Mapping was done, but the crash is still here");
      }

      siginfo_t siginfo;
      ptrace(PTRACE_GETSIGINFO, child, nullptr, &siginfo);
      struct user_regs_struct regs;
      ptrace(PTRACE_GETREGS, child, nullptr, &regs);
      if (siginfo.si_signo != 11 && siginfo.si_signo != 5)
        return llvm::createStringError(
            std::make_error_code(std::errc::invalid_argument),
            "Incorrect signal %i", siginfo.si_signo);

      void *pageFaultAddress = siginfo.si_addr;
      void *restartAddress = (void *)&map_and_restart;

      gCmd->addr = pageFaultAddress;

      if ((void *)regs.rip == lastSignaledInstruction)
        return llvm::createStringError(
            std::make_error_code(std::errc::invalid_argument),
            "Same instruction failed twice: %x", regs.rip);

      lastSignaledInstruction = (void *)regs.rip;

      restartChild(child, restartAddress);
    }

    kill(child, SIGKILL);
    close(shmemFD);
  }

  return llvm::createStringError(
      std::make_error_code(std::errc::invalid_argument), "Unknown error");
}
} // namespace llvm_ml::detail
