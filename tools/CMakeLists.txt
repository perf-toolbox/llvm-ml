add_llvm_library(llvm_mc_bench_lib
        "llvm-mc-bench/BenchmarkGenerator.cpp"
        "llvm-mc-bench/BenchmarkGenerator.hpp"
        "llvm-mc-bench/BenchmarkResult.cpp"
        "llvm-mc-bench/BenchmarkResult.hpp"
        "llvm-mc-bench/BenchmarkRunner.hpp"
        "llvm-mc-bench/counters.cpp"
        "llvm-mc-bench/counters.hpp"
        "llvm-mc-bench/cpu_benchmark_runner_macos.cpp"
  LINK_COMPONENTS
  MC
  OrcJIT
  Target
  AllTargetsAsmParsers
  AllTargetsCodeGens
  AllTargetsDescs
  AllTargetsInfos
  Support
)
target_compile_options(llvm_mc_bench_lib PUBLIC -fexceptions)
target_link_libraries(llvm_mc_bench_lib PUBLIC
        "pmu"
        "cpp_structures"
        "target"
        "nlohmann_json"
        "range-v3"
)

target_include_directories(llvm_mc_bench_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/llvm-mc-bench
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/target
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/structures
)
target_include_directories(llvm_mc_bench_lib PUBLIC ${LIBPMU_INCLUDE_DIR})
target_link_directories(llvm_mc_bench_lib PUBLIC ${LIBPMU_LIB_DIR})

add_executable(llvm-mc-bench
    "llvm-mc-bench/llvm-mc-bench.cpp"
)
target_link_libraries(llvm-mc-bench PRIVATE llvm_mc_bench_lib)
target_include_directories(llvm-mc-bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/indicators/single_include)
