load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("//bazel:packaging_utils.bzl", "shared_object_path_selector")

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
      "@platforms//os:macos": [
        "llvm-mc-bench/cpu_benchmark_runner_macos.cpp",
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
      "@llvm-ml-libs//:target",
      "@llvm-ml-libs//:cpp_structures",
      "//third_party:libpmu",
      "@//third_party:indicators",
      "@range-v3",
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
      "@llvm-ml-libs//:target",
    ],
    visibility = [
      "//tests:__pkg__",
    ],
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
      "@llvm-ml-libs//:target",
      "@llvm-ml-libs//:graph",
      "@//third_party:indicators",
    ],
    visibility = [
      "//tests:__pkg__",
    ],
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
      "@llvm-ml-libs//:target",
      "@llvm-ml-libs//:graph",
      "@llvm-ml-libs//:cpp_structures",
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
        "@llvm-ml-libs//:cpp_structures",
        "@llvm-ml-libs//:statistics",
        "@range-v3",
    ],
    visibility = [
      "//tests:__pkg__",
    ],
)

pkg_files(
    name = "binary",
    srcs = [
        ":llvm-mc-bench",  
        ":llvm-mc-extract",  
        ":llvm-mc-embedding",  
        ":llvm-mc-dataset",  
    ],
    prefix = select(shared_object_path_selector) + "/bin",
    visibility = ["//visibility:public"]
)
