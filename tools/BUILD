cc_library(
    name = "llvm-mc-bench-lib",
    hdrs = [
      "llvm-mc-bench/counters.hpp",
      "llvm-mc-bench/BenchmarkResult.hpp",
      "llvm-mc-bench/BenchmarkRunner.hpp",
      "llvm-mc-bench/BenchmarkGenerator.hpp",
    ],
    srcs = [
      "llvm-mc-bench/counters.hpp",
      "llvm-mc-bench/BenchmarkResult.hpp",
      "llvm-mc-bench/BenchmarkRunner.hpp",
      "llvm-mc-bench/BenchmarkResult.cpp",
      "llvm-mc-bench/BenchmarkGenerator.hpp",
      "llvm-mc-bench/BenchmarkGenerator.cpp",
      "llvm-mc-bench/counters.cpp",
    ] + select({
      "@platforms//os:linux": [
        "llvm-mc-bench/cpu_benchmark_runner_linux.cpp",
      ],
      "//conditions:default": [],
    }),
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@llvm-project//llvm:OrcJIT",
      "@nlohmann_json//:json",
      "@//lib:target",
      "@//lib:cpp_structures_proto",
      "//third_party:libpmu",
    ],
    visibility = ["//tools:__pkg__", "//utils:__pkg__"],
)

cc_binary(
    name = "llvm-mc-bench",
    srcs = [
      "llvm-mc-bench/llvm-mc-bench.cpp",
    ],
    deps = [
      "@//tools:llvm-mc-bench-lib",
      "@nlohmann_json//:json",
      "@//lib:target",
      "@//lib:cpp_structures_proto",
    ]
)

cc_binary(
    name = "llvm-mc-extract",
    srcs = [
      "llvm-mc-extract/llvm-mc-extract.cpp",
    ],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsDisassemblers",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@//lib:target",
      "@//third_party:indicators",
    ]
)

cc_binary(
    name = "llvm-mc-embedding",
    srcs = [
      "llvm-mc-embedding/llvm-mc-embedding.cpp",
    ],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@nlohmann_json//:json",
      "@//lib:target",
      "@//lib:cpp_structures_proto",
      "@//third_party:indicators",
    ]
)

cc_binary(
    name = "llvm-mc-dataset",
    srcs = [
      "llvm-mc-dataset/llvm-mc-dataset.cpp",
    ],
    deps = [
      "@llvm-project//llvm:Support",
      "@//lib:cpp_structures_proto",
    ]
)
