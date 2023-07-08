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
#include "llvm/Support/FormatVariadic.h"
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

  llvm::ArrayRef<llvm_ml::BenchmarkResult> getNoiseResults() const override {
    return mNoiseResults;
  }

  llvm::ArrayRef<llvm_ml::BenchmarkResult> getWorkloadResults() const override {
    return mWorkloadResults;
  }

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

static int allocateSharedMemory(const std::string &path, int flag) {
  int mode = [flag]() {
    if ((flag & O_CREAT) != 0) {
      return S_IRUSR | S_IWUSR | S_IXUSR;
    }
    return 0;
  }();
  int fd = shm_open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (errno != 0) {
    llvm::errs() << "Failed to open shared memory: " << strerror(errno) << "\n";
    llvm::errs() << "Path is " << path << "\n";
    abort();
  }
  if (mode != 0 && ftruncate(fd, 4 * PAGE_SIZE) != 0) {
    llvm::errs() << "Failed to truncate shmem file: " << strerror(errno)
                 << "\n";
    llvm::errs() << "Path is " << path << "\n";
  }
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

  int shmemFD = allocateSharedMemory(shmemPath, O_RDWR);
  void *out = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   shmemFD, PAGE_SIZE * 2);

  for (auto addr : addresses) {
    void *res = nullptr;
    if (addr == (void *)0x2325000) {
      res = mmap(addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, shmemFD, PAGE_SIZE);
    } else {
      // FIXME(Alex): 12 should be std::bit_width(PAGE_ADDR)
      size_t shift = reinterpret_cast<size_t>(addr) >> 12;
      void *pageAddr = reinterpret_cast<void *>(shift << 12);
      res = mmap(pageAddr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, shmemFD, 0);
    }
    if (errno != 0) {
      llvm::errs() << strerror(errno) << "\n";
    }
    if (!res) {
      llvm::errs() << "Failed to map address\n";
      abort();
    }
  }

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

  llvm_ml::flushCounters(counters.get());
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
  return siginfo.si_signo == SIGSEGV || siginfo.si_signo == SIGTRAP;
}

static std::pair<void *, void *> getSegfaultAddr(int child) {
  siginfo_t siginfo;
  ptrace(PTRACE_GETSIGINFO, child, nullptr, &siginfo);
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, child, nullptr, &regs);

  void *pageFaultAddress = siginfo.si_addr;
#if defined(__amd64__)
  void *signaledInstruction = (void *)regs.rip;
#else
#error "Unsupported platform"
#endif

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

  std::string shmemPath = llvm::formatv("/llvm-mc-bench-{0}.shmem", getpid());

  int shmemFD = allocateSharedMemory(shmemPath, O_RDWR | O_CREAT | O_EXCL);
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

    for (int i = 0; i < mNumRuns; i++) {
      ExitStatus status = fork<ExitStatus>(
          [&]() {
            runHarness(libPath, harnessName, pinnedCPU, shmemPath,
                       mappedAddresses);
          },
          [](int child) -> ExitStatus { return runParent(child); });

      if (status.reason != ExitReason::Success) {
        llvm::errs() << "Run failed\n";
        results.push_back(BenchmarkResult{.hasFailed = true});
      } else {
        void *out = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                         shmemFD, PAGE_SIZE * 2);
        uint64_t *res = reinterpret_cast<uint64_t *>(
            static_cast<size_t *>(out) + 2 * sizeof(void *));
        results.push_back(BenchmarkResult{.hasFailed = false,
                                          .numCycles = res[0],
                                          .numContextSwitches = res[2],
                                          .numCacheMisses = res[1],
                                          .numMicroOps = res[4],
                                          .numInstructions = res[3],
                                          .numMisalignedLoads = res[5]});
      }
    }

    return llvm::Error::success();
  };

  if (auto err = runBenchmark(llvm_ml::kBaselineNoiseName, 1, mNoiseResults))
    return err;
  if (auto err = runBenchmark(llvm_ml::kWorkloadName, 1, mWorkloadResults))
    return err;

  return llvm::Error::success();
}

namespace llvm_ml {
std::unique_ptr<BenchmarkRunner>
createCPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                         std::unique_ptr<llvm::Module> module,
                         size_t repeatNoise, size_t repeatWorkload,
                         int numRuns) {
  assert(target);
  return std::make_unique<CPUBenchmarkRunner>(target, tripleName,
                                              std::move(module), repeatNoise,
                                              repeatWorkload, numRuns);
}
} // namespace llvm_ml
