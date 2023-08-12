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

#include <chrono>
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
constexpr uint64_t kTimeSliceNS = 3'000'000;

using namespace llvm_ml;

namespace {
class CPUBenchmarkRunner : public BenchmarkRunner {
public:
  CPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                     int pinnedCPU, int numRuns)
      : mTarget(target), mTripleName(tripleName), mPinnedCPU(pinnedCPU),
        mNumRuns(numRuns) {}

  llvm::Error run(std::unique_ptr<llvm::Module> harness, size_t numNoiseRepeat,
                  size_t numRepeat) override;
  llvm::Expected<int> check(std::unique_ptr<llvm::Module> harness,
                            size_t numNoiseRepeat) override;

  llvm::ArrayRef<llvm_ml::BenchmarkResult> getNoiseResults() const override {
    return mNoiseResults;
  }

  llvm::ArrayRef<llvm_ml::BenchmarkResult> getWorkloadResults() const override {
    return mWorkloadResults;
  }

private:
  llvm::Error
  runSingleBenchmark(const std::string &libPath, const std::string &harnessName,
                     int numRepeat, const std::string &shmemPath, int shmemFD,
                     llvm::SmallVectorImpl<void *> &mappedAddresses,
                     llvm::SmallVectorImpl<llvm_ml::BenchmarkResult> &results);
  llvm::Expected<std::string> compile(std::unique_ptr<llvm::Module> harness);

  const llvm::Target *mTarget;
  llvm::StringRef mTripleName;
  int mPinnedCPU;
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
      return 0666;
    }
    return 0;
  }();
  int fd = shm_open(path.c_str(), flag, mode);
  if (fd == -1) {
    llvm::errs() << "Failed to open shared memory: " << strerror(errno) << "\n";
    llvm::errs() << "Path is " << path << "\n";
    abort();
  }
  if (mode != 0 && ftruncate(fd, 4 * PAGE_SIZE) != 0) {
    llvm::errs() << "Failed to truncate shmem file: " << strerror(errno)
                 << "\n";
    llvm::errs() << "Path is " << path << "\n";
    abort();
  }
  return fd;
}

static void fake_bench(void *) {}

static void runHarness(llvm::StringRef libPath, std::string harnessName,
                       int pinnedCPU, const std::string &shmemPath, int numRuns,
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
  // Silently ignore return error in non-root mode
  sched_setscheduler(0, SCHED_FIFO, &schedParam);

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
    if (!res) {
      llvm::errs() << "Failed to map address: " << strerror(errno) << "\n";
      abort();
    }

    __builtin_prefetch(addr, 0, 0);
  }

  size_t runId = 0;
  auto counters =
      createCounters([out, &runId](llvm::ArrayRef<CounterValue> values) {
        BenchmarkResult result{.hasFailed = false};
        for (const auto &val : values) {
          if (val.type == Counter::Cycles) {
            result.numCycles = val.value;
          } else if (val.type == Counter::CacheMisses) {
            result.numCacheMisses = val.value;
          } else if (val.type == Counter::ContextSwitches) {
            result.numContextSwitches = val.value;
          } else if (val.type == Counter::Instructions) {
            result.numInstructions = val.value;
          } else if (val.type == Counter::MicroOps) {
            result.numMicroOps = val.value;
          } else if (val.type == Counter::MisalignedLoads) {
            result.numMisalignedLoads = val.value;
          }
        }

        auto *res = reinterpret_cast<BenchmarkResult *>(out);
        res[runId++] = result;
      });

  // Warm-up
  for (size_t i = 0; i < 5; i++)
    fn(nullptr, reinterpret_cast<void *>(&fake_bench),
       reinterpret_cast<void *>(&fake_bench), out);

  for (int i = 0; i < numRuns; i++) {
    // Voluntarily give up processor time to get a full CPU slice.
    std::this_thread::yield();
    auto start = std::chrono::high_resolution_clock::now();
    fn(counters.get(), reinterpret_cast<void *>(&counters_start),
       reinterpret_cast<void *>(&counters_stop), out);
    auto end = std::chrono::high_resolution_clock::now();

    llvm_ml::flushCounters(counters.get());

    uint64_t duration = std::chrono::nanoseconds(end - start).count();
    auto *res = reinterpret_cast<BenchmarkResult *>(out);
    res[i].wallTime = duration;
  }

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

llvm::Expected<std::string>
CPUBenchmarkRunner::compile(std::unique_ptr<llvm::Module> module) {
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
  module->setDataLayout(dl);

  llvm::legacy::PassManager pass;
  auto fileType = llvm::CGFT_ObjectFile;

  if (tm->addPassesToEmitFile(pass, objOs, nullptr, fileType)) {
    llvm::errs() << "TargetMachine can't emit a file of this type";
    std::terminate();
  }

  pass.run(*module);
  objOs.flush();
  objOs.close();

  llvm::SmallVector<char> libPathChar;
  llvm::sys::fs::createUniquePath("llvm-mc-bench-%%%%%%.so", libPathChar, true);
  std::string libPath{libPathChar.begin(), libPathChar.end()};
  auto rmLib = llvm::make_scope_exit([&] { llvm::sys::fs::remove(libPath); });

  std::string command = "ld -shared -o " + libPath + " " + objPath;

  if (std::system(command.c_str()) != 0) {
    return llvm::createStringError(std::errc::invalid_argument,
                                   "Failed to link dynamic library");
  }

  return libPath;
}

llvm::Error CPUBenchmarkRunner::runSingleBenchmark(
    const std::string &libPath, const std::string &harnessName, int numRepeat,
    const std::string &shmemPath, int shmemFD,
    llvm::SmallVectorImpl<void *> &mappedAddresses,
    llvm::SmallVectorImpl<llvm_ml::BenchmarkResult> &results) {
  void *lastSignaledInstruction = nullptr;

  for (unsigned i = 0; i < MAX_FAULTS; i++) {
    ExitStatus status = fork<ExitStatus>(
        [&]() {
          runHarness(libPath, harnessName, mPinnedCPU, shmemPath, 1,
                     mappedAddresses);
        },
        [](int child) -> ExitStatus { return runParent(child); });

    if (status.reason == ExitReason::Unknown) {
      return llvm::createStringError(std::errc::executable_format_error,
                                     "Pre-run failed for unknown reason");
    }

    if (status.reason == ExitReason::Segfault) {
      if (status.ip == lastSignaledInstruction) {
        return llvm::createStringError(std::errc::executable_format_error,
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

  for (size_t i = 0; i < MAX_FAULTS; i++) {
    ExitStatus status = fork<ExitStatus>(
        [&]() {
          runHarness(libPath, harnessName, mPinnedCPU, shmemPath, mNumRuns,
                     mappedAddresses);
        },
        [](int child) -> ExitStatus { return runParent(child); });

    if (status.reason != ExitReason::Success) {
      results.push_back(BenchmarkResult{.hasFailed = true});
      continue;
    }

    void *out = mmap(nullptr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shmemFD, PAGE_SIZE * 2);
    auto *res = reinterpret_cast<BenchmarkResult *>(out);
    for (int j = 0; j < mNumRuns; j++) {
      results.push_back(res[j]);
      results.back().numRuns = numRepeat;
    }
  }

  return llvm::Error::success();
}

llvm::Expected<int>
CPUBenchmarkRunner::check(std::unique_ptr<llvm::Module> harness,
                          size_t numNoiseRepeat) {
  assert(sizeof(BenchmarkResult) * mNumRuns < PAGE_SIZE);

  auto maybeLibPath = compile(std::move(harness));

  if (!maybeLibPath)
    return maybeLibPath.takeError();

  std::string shmemPath =
      llvm::formatv("/llvm-mc-bench-{0}-{1}.shmem", getpid(), rand());

  int shmemFD = allocateSharedMemory(shmemPath, O_RDWR | O_CREAT | O_EXCL);
  auto shmemRemove =
      llvm::make_scope_exit([&]() { shm_unlink(shmemPath.c_str()); });

  llvm::SmallVector<void *> mappedAddresses;
  llvm::SmallVector<llvm_ml::BenchmarkResult> results;

  if (auto err = runSingleBenchmark(*maybeLibPath, llvm_ml::kBaselineNoiseName,
                                    numNoiseRepeat, shmemPath, shmemFD,
                                    mappedAddresses, results))
    return err;

  const auto minEltPred = [](const auto &lhs, const auto &rhs) {
    return lhs.numCycles < rhs.numCycles;
  };

  auto minNoise = std::min_element(results.begin(), results.end(), minEltPred);

  uint64_t avgNSPerIter = minNoise->wallTime / minNoise->numRuns;
  int numRepeat = static_cast<int>((kTimeSliceNS / avgNSPerIter) * 0.8f);

  return numRepeat;
}

llvm::Error CPUBenchmarkRunner::run(std::unique_ptr<llvm::Module> harness,
                                    size_t numNoiseRepeat, size_t numRepeat) {
  assert(sizeof(BenchmarkResult) * mNumRuns < PAGE_SIZE);

  auto maybeLibPath = compile(std::move(harness));

  if (!maybeLibPath)
    return maybeLibPath.takeError();

  std::string shmemPath = llvm::formatv("/llvm-mc-bench-{0}.shmem", getpid());

  int shmemFD = allocateSharedMemory(shmemPath, O_RDWR | O_CREAT | O_EXCL);
  auto shmemRemove =
      llvm::make_scope_exit([&]() { shm_unlink(shmemPath.c_str()); });

  llvm::SmallVector<void *> mappedAddresses;

  if (auto err = runSingleBenchmark(*maybeLibPath, llvm_ml::kBaselineNoiseName,
                                    numNoiseRepeat, shmemPath, shmemFD,
                                    mappedAddresses, mNoiseResults))
    return err;
  if (auto err = runSingleBenchmark(*maybeLibPath, llvm_ml::kWorkloadName,
                                    numRepeat, shmemPath, shmemFD,
                                    mappedAddresses, mWorkloadResults))
    return err;

  return llvm::Error::success();
}

namespace llvm_ml {
std::unique_ptr<BenchmarkRunner>
createCPUBenchmarkRunner(const llvm::Target *target, llvm::StringRef tripleName,
                         int pinnedCPU, int numRuns) {
  assert(target);
  return std::make_unique<CPUBenchmarkRunner>(target, tripleName, pinnedCPU,
                                              numRuns);
}
} // namespace llvm_ml
