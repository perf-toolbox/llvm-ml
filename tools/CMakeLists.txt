add_library(llvm_mc_bench_lib
        "llvm-mc-bench/BenchmarkGenerator.cpp"
        "llvm-mc-bench/BenchmarkGenerator.hpp"
        "llvm-mc-bench/BenchmarkResult.cpp"
        "llvm-mc-bench/BenchmarkResult.hpp"
        "llvm-mc-bench/BenchmarkRunner.hpp"
        "llvm-mc-bench/counters.cpp"
        "llvm-mc-bench/counters.hpp"
        "llvm-mc-bench/cpu_benchmark_runner_macos.cpp"
)
target_link_libraries(llvm_mc_bench_lib PUBLIC
        "libpmu"
        "cpp_structures"
        "target"
        "indicators"
        "LLVMAllTargetsAsmParsers"
        "LLVMMC"
        "LLVMOrcJIT"
        "LLVMSupport"
        "LLVMTarget"
        "nlohmann_json"
        "range-v3"
    ],
)

target_include_directories(llvm_mc_bench_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/llvm-mc-bench
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/target
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/structures
)
target_include_directories(llvm_mc_bench_lib PUBLIC ${LIBPMU_INCLUDE_DIR})
target_link_directories(llvm_mc_bench_lib PUBLIC ${LIBPMU_LIB_DIR})

