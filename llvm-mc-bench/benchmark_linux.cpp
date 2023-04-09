//===--- benchmark_linux.cpp - Benchmark runner -----------------------C++-===//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "llvm/Support/Error.h"

#include "benchmark.hpp"
#include "counters.hpp"

constexpr unsigned MAX_FAULTS = 30;
constexpr size_t INIT_VALUE = 0x2324000;
constexpr size_t ADDR_OF_AUX_MEM = 0x0000700000000000;

struct PageFaultCommand {
  void *addr;
  bool unmapFirst;
};

int gSharedMem = 42;
size_t gRunnerEnd = 0;
llvm_ml::BenchmarkFn gBenchFn = nullptr;
llvm_ml::CountersContext *gCont;
PageFaultCommand *gCmd;

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

  (void)mmap(gCmd->addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
             gSharedMem, 0);

  gBenchFn(gCont);
}

static void unmap_unused_pages() {
  std::array<std::pair<size_t, size_t>, 13> ptrs = {
      std::make_pair(reinterpret_cast<size_t>(&unmap_unused_pages),
                     (size_t) && label + PAGE_SIZE),
      {reinterpret_cast<size_t>(&gBenchFn), 0},
      {reinterpret_cast<size_t>(&llvm_ml::run_benchmark),
       gRunnerEnd + PAGE_SIZE},
      {reinterpret_cast<size_t>(&counters_start), 0},
      {reinterpret_cast<size_t>(&counters_stop), 0},
      {reinterpret_cast<size_t>(&counters_cycles), 0},
      {reinterpret_cast<size_t>(&mmap),
       reinterpret_cast<size_t>(&mmap) + 4 * PAGE_SIZE},
      {reinterpret_cast<size_t>(&munmap),
       reinterpret_cast<size_t>(&munmap) + 4 * PAGE_SIZE},
      {reinterpret_cast<size_t>(&llvm_ml::counters_free), 0},
      {reinterpret_cast<size_t>(&llvm_ml::counters_init), 0},
      {reinterpret_cast<size_t>(&close), 0},
      {reinterpret_cast<size_t>(&_exit), 0},
      {reinterpret_cast<size_t>(&gSharedMem), 0},
  };

  for (auto &iptr : ptrs) {
    alignPtr(iptr.first);
    alignPtr(iptr.second);
  }

  std::sort(ptrs.begin(), ptrs.end(),
            [](auto l, auto r) { return l.first < r.first; });

  constexpr size_t maxPage = 0x0000700000001000;

  size_t curPtr = 0;
  for (auto &iptr : ptrs) {
    munmap(reinterpret_cast<void *>(curPtr), iptr.first - curPtr);
    curPtr = (iptr.second == 0 ? iptr.first : iptr.second) + PAGE_SIZE;
  }
  munmap(reinterpret_cast<void *>(curPtr + PAGE_SIZE),
         maxPage - curPtr + PAGE_SIZE);
label:
  return;
}
}

namespace llvm_ml {

static void restart_child(pid_t pid, void *restart_addr, void *fault_addr,
                          int shm_fd) {
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, NULL, &regs);
  regs.rip = (unsigned long)restart_addr;
  // regs.rax = (unsigned long)fault_addr;
  // regs.r11 = shm_fd;
  // regs.r12 = MAP_SHARED;
  // regs.r13 = PROT_READ|PROT_WRITE;
  ptrace(PTRACE_SETREGS, pid, NULL, &regs);
  ptrace(PTRACE_CONT, pid, NULL, NULL);
}

static int allocate_shmem() {
  constexpr auto path = "shmem-path";
  int fd = shm_open(path, O_RDWR | O_CREAT, 0777);
  shm_unlink(path);
  ftruncate(fd, 4 * PAGE_SIZE);
  return fd;
}

llvm::Error run_benchmark(BenchmarkFn bench, const std::string &outFile) {
  int status;
  int shmemFD = allocate_shmem();

  pid_t child = fork();

  if (child == 0) {
    sleep(1);
    gBenchFn = bench;
    gRunnerEnd = (size_t) && end;
    dup2(shmemFD, 42);
    signal(SIGSEGV, SIG_DFL);
    void *outPtr =
        mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, shmemFD, 4096);
    gCmd = (PageFaultCommand *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, shmemFD, 4096 * 2);
    // unmap_unused_pages();
    gCont = llvm_ml::counters_init();
    size_t cycles = bench(gCont);
    llvm_ml::counters_free(gCont);

    *static_cast<size_t *>(outPtr) = cycles;

    close(shmemFD);

    _exit(0);
  } else { // close(fds[0]);
    signal(SIGSEGV, SIG_DFL);

    gCmd = (PageFaultCommand *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, shmemFD, 4096 * 2);
    // void *physPage = mmap(nullptr, PAGE_SIZE, PROT_READ|PROT_WRITE,
    // MAP_SHARED, shmemFD, 0); (void)physPage;

    ptrace(PTRACE_SEIZE, child, NULL, NULL);

    char *last_failing_inst = 0;
    bool mappingDone = false;

    for (unsigned i = 0; i < MAX_FAULTS; i++) {
      child = wait(&status);

      if (WIFEXITED(status) || child == -1) {
        if (WEXITSTATUS(status) == 0) {
          void *outPtr = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                              shmemFD, 4096);
          size_t cycles = *static_cast<size_t *>(outPtr);
          llvm::dbgs() << "Cycles total : " << cycles << "\n";
          return llvm::Error::success();
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

      void *fault_addr = siginfo.si_addr;
      void *restart_addr = (void *)&map_and_restart;
      if (siginfo.si_signo == 5)
        fault_addr = (void *)INIT_VALUE;

      gCmd->addr = fault_addr;

      if ((char *)regs.rip == last_failing_inst)
        return llvm::createStringError(
            std::make_error_code(std::errc::invalid_argument),
            "Same instruction failed twice");

      last_failing_inst = (char *)regs.rip;

      restart_child(child, restart_addr, fault_addr, shmemFD);
    }

    kill(child, SIGKILL);
    close(shmemFD);
  }

end:
  return llvm::createStringError(
      std::make_error_code(std::errc::invalid_argument), "Unknown error");
}
} // namespace llvm_ml
