//===--- cpu_benchmark_runner_linux.cpp - Benchmark running tool ----------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "BenchmarkGenerator.hpp"
#include "BenchmarkResult.hpp"
#include "BenchmarkRunner.hpp"
#include "counters.hpp"

#include "llvm/ADT/ScopeExit.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <functional>
#include <string>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <thread>

constexpr unsigned MAX_FAULTS = 30;

using namespace llvm_ml;

namespace {
class CPUBenchmarkRunner : public BenchmarkRunner {
public:
  CPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                     std::unique_ptr<llvm::Module> module, int repeatNoise,
                     int repeatWorkload, int numRuns)
      : mTarget(target), mTripleName(tripleName), mModule(std::move(module)),
        mRepeatNoise(repeatNoise), mRepeatWorkload(repeatWorkload),
        mNumRuns(numRuns) {}

  llvm::Error run() override;

private:
  const llvm::Target *mTarget;
  llvm::StringRef mTripleName;
  std::unique_ptr<llvm::Module> mModule;
  int mRepeatNoise;
  int mRepeatWorkload;
  int mNumRuns;
  llvm::SmallVector<llvm_ml::BenchmarkResult> mNoiseResults;
  llvm::SmallVector<llvm_ml::BenchmarkResult> mWorkloadResults;
};
} // namespace

template <typename ParentResT>
ParentResT fork(std::function<void()> forkBody,
                std::function<ParentResT(int)> parentBody) {
  pid_t child = fork();

  if (child == 0) {
    forkBody();
    _exit(0);
  } else {
    return parentBody(child);
  }

  llvm_unreachable("Failed to fork");
}

static int allocateSharedMemory(const std::string &path) {
  int fd = shm_open(path.c_str(), O_RDWR | O_CREAT, 0777);
  ftruncate(fd, 4 * PAGE_SIZE);
  return fd;
}

static void runHarness(llvm::StringRef libPath, std::string harnessName,
                       int pinnedCPU, const std::string &shmemPath,
                       llvm::ArrayRef<void *> addresses) {
  // FIXME(Alex): there must be a better way to wait for parent to become
  // ready
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  void *lib = dlopen(libPath.data(), RTLD_NOW);
  if (!lib)
    _exit(1);

  auto fn =
      reinterpret_cast<llvm_ml::BenchmarkFn>(dlsym(lib, harnessName.c_str()));
  if (!fn)
    _exit(1);

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

  // LLVM installs its own segfault handler. We don't need that.
  signal(SIGSEGV, SIG_DFL);

  int shmemFD = allocateSharedMemory(shmemPath);
  void *out = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   shmemFD, PAGE_SIZE * 2);

  auto counters = createCounters([out](llvm::ArrayRef<CounterValue> values) {
    uint64_t *res = reinterpret_cast<uint64_t *>(static_cast<size_t *>(out) +
                                                 2 * sizeof(void *));
    for (const auto &val : values) {
      if (val.type == Counter::Cycles) {
        res[0] = val.value;
      } else if (val.type == Counter::CacheMisses) {
        res[1] = val.value;
      } else if (val.type == Counter::ContextSwitches) {
        res[2] = val.value;
      } else if (val.type == Counter::Instructions) {
        res[3] = val.value;
      } else if (val.type == Counter::MicroOps) {
        res[4] = val.value;
      } else if (val.type == Counter::MisalignedLoads) {
        res[5] = val.value;
      }
    }
  });

  fn(counters.get(), reinterpret_cast<void *>(&counters_start),
     reinterpret_cast<void *>(&counters_stop), out);

  counters->flush();
  close(shmemFD);
  _exit(0);
}

enum class ExitReason {
  Success,
  Segfault,
  Unknown,
};

struct ExitStatus {
  ExitReason reason;
  void *memAddr;
  void *ip;
};

static bool isSegfault(int child) {
  siginfo_t siginfo;
  ptrace(PTRACE_GETSIGINFO, child, nullptr, &siginfo);
  return siginfo.si_signo == 11 || siginfo.si_signo == 5;
}

static std::pair<void *, void *> getSegfaultAddr(int child) {
  siginfo_t siginfo;
  ptrace(PTRACE_GETSIGINFO, child, nullptr, &siginfo);
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, child, nullptr, &regs);

  void *pageFaultAddress = siginfo.si_addr;
  void *signaledInstruction = (void *)regs.rip;

  return std::make_pair(pageFaultAddress, signaledInstruction);
}

static ExitStatus runParent(int child) {
  ptrace(PTRACE_SEIZE, child, NULL, NULL);

  int status;
  int result = wait(&status);

  if (WIFEXITED(status) || result == -1) {
    if (WEXITSTATUS(status) == 0) {
      return ExitStatus{.reason = ExitReason::Success, .memAddr = 0, .ip = 0};
    }
  }

  if (!isSegfault(child))
    return ExitStatus{.reason = ExitReason::Unknown, .memAddr = 0, .ip = 0};

  auto [pageFaultAddress, signaledInstruction] = getSegfaultAddr(child);

  return ExitStatus{.reason = ExitReason::Segfault,
                    .memAddr = pageFaultAddress,
                    .ip = signaledInstruction};
}

llvm::Error CPUBenchmarkRunner::run() {
  int objFd = 0;
  llvm::SmallVector<char> objectPathChar;
  llvm::sys::fs::createTemporaryFile("llvm-mc-bench", ".o", objFd,
                                     objectPathChar);
  std::string objPath{objectPathChar.begin(), objectPathChar.end()};

  auto rmObj = llvm::make_scope_exit([&] { llvm::sys::fs::remove(objPath); });

  llvm::raw_fd_ostream objOs(objFd, true);

  llvm::TargetMachine *tm = mTarget->createTargetMachine(
      mTripleName, "generic", "", llvm::TargetOptions{}, std::nullopt);

  auto dl = tm->createDataLayout();
  mModule->setDataLayout(dl);

  llvm::legacy::PassManager pass;
  auto fileType = llvm::CGFT_ObjectFile;

  if (tm->addPassesToEmitFile(pass, objOs, nullptr, fileType)) {
    llvm::errs() << "TargetMachine can't emit a file of this type";
    std::terminate();
  }

  pass.run(*mModule);
  objOs.flush();
  objOs.close();

  llvm::SmallVector<char> libPathChar;
  llvm::sys::fs::createUniquePath("llvm-mc-bench-%%%%%%.so", libPathChar, true);
  std::string libPath{libPathChar.begin(), libPathChar.end()};
  auto rmLib = llvm::make_scope_exit([&] { llvm::sys::fs::remove(libPath); });

  std::string command = "ld -shared -o " + libPath + " " + objPath;

  if (std::system(command.c_str()) != 0) {
    llvm::errs() << "Failed to link dynamic library\n";
    std::terminate();
  }

  llvm::SmallVector<char> shmemPathChar;
  llvm::sys::fs::createUniquePath("llvm-mc-bench-%%%%%%.shmem", shmemPathChar,
                                  true);
  std::string shmemPath{shmemPathChar.begin(), shmemPathChar.end()};

  int shmemFD = allocateSharedMemory(shmemPath);
  auto shmemRemove =
      llvm::make_scope_exit([&]() { shm_unlink(shmemPath.c_str()); });

  llvm::SmallVector<void *> mappedAddresses;

  const auto runBenchmark =
      [&](const std::string &harnessName, int pinnedCPU,
          llvm::SmallVectorImpl<llvm_ml::BenchmarkResult> &results)
      -> llvm::Error {
    void *lastSignaledInstruction = nullptr;

    for (unsigned i = 0; i < MAX_FAULTS; i++) {
      ExitStatus status = fork<ExitStatus>(
          [&]() {
            runHarness(libPath, harnessName, pinnedCPU, shmemPath,
                       mappedAddresses);
          },
          [](int child) -> ExitStatus { return runParent(child); });

      if (status.reason == ExitReason::Unknown) {
        return llvm::createStringError(std::errc::executable_format_error,
                                       "Pre-run failed for unknown reason");
      }

      if (status.reason == ExitReason::Segfault) {
        if (status.ip == lastSignaledInstruction) {
          return llvm::createStringError(
              std::errc::executable_format_error,
              "The same instruction segfaulted twice");
        }

        if (status.memAddr == nullptr) {
          return llvm::createStringError(std::errc::executable_format_error,
                                         "Attempt to access nullptr");
        }

        lastSignaledInstruction = status.ip;

        mappedAddresses.push_back(status.memAddr);
      }
    }

    for (unsigned i = 0; i < mNumRuns; i++) {
      ExitStatus status = fork<ExitStatus>(
          [&]() {
            runHarness(libPath, harnessName, pinnedCPU, shmemPath,
                       mappedAddresses);
          },
          [](int child) -> ExitStatus { return runParent(child); });

      if (status.reason != ExitReason::Success) {
        results.push_back(BenchmarkResult{.hasFailed = true});
      }
    }

    return llvm::Error::success();
  };

  if (auto err = runBenchmark(llvm_ml::kBaselineNoiseName, 1, mNoiseResults))
    return err;
  if (auto err = runBenchmark(llvm_ml::kWorkloadName, 1, mNoiseResults))
    return err;

  return llvm::Error::success();
}

namespace llvm_ml {
std::unique_ptr<BenchmarkRunner>
createCPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                         std::unique_ptr<llvm::Module> module,
                         size_t repeatNoise, size_t repeatWorkload) {
  assert(target);
  return std::make_unique<CPUBenchmarkRunner>(target, tripleName,
                                              std::move(module), repeatNoise,
                                              repeatWorkload, numRuns);
}
} // namespace llvm_ml
