load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("//bazel:packaging_utils.bzl", "shared_object_path_selector")

cc_library(
    name = "llvm-mc-bench-lib",
    srcs = [
        "llvm-mc-bench/BenchmarkGenerator.cpp",
        "llvm-mc-bench/BenchmarkGenerator.hpp",
        "llvm-mc-bench/BenchmarkResult.cpp",
        "llvm-mc-bench/BenchmarkResult.hpp",
        "llvm-mc-bench/BenchmarkRunner.hpp",
        "llvm-mc-bench/counters.cpp",
        "llvm-mc-bench/counters.hpp",
    ] + select({
        "@platforms//os:linux": [
            "llvm-mc-bench/cpu_benchmark_runner_linux.cpp",
        ],
        "@platforms//os:macos": [
            "llvm-mc-bench/cpu_benchmark_runner_macos.cpp",
        ],
        "//conditions:default": [],
    }),
    hdrs = [
        "llvm-mc-bench/BenchmarkGenerator.hpp",
        "llvm-mc-bench/BenchmarkResult.hpp",
        "llvm-mc-bench/BenchmarkRunner.hpp",
        "llvm-mc-bench/counters.hpp",
    ],
    visibility = [
        "//tools:__pkg__",
        "//utils:__pkg__",
    ],
    deps = [
        "//third_party:libpmu",
        "@//lib:cpp_structures",
        "@//lib:target",
        "@//third_party:indicators",
        "@llvm-project//llvm:AllTargetsAsmParsers",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:OrcJIT",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
        "@nlohmann_json//:json",
        "@range-v3",
    ],
)

cc_binary(
    name = "llvm-mc-bench",
    srcs = [
        "llvm-mc-bench/llvm-mc-bench.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
    ],
    deps = [
        "@//lib:target",
        "@//tools:llvm-mc-bench-lib",
        "@nlohmann_json//:json",
    ],
)

cc_binary(
    name = "llvm-mc-extract",
    srcs = [
        "llvm-mc-extract/llvm-mc-extract.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
    ],
    deps = [
        "@//lib:graph",
        "@//lib:target",
        "@//third_party:indicators",
        "@llvm-project//llvm:AllTargetsAsmParsers",
        "@llvm-project//llvm:AllTargetsDisassemblers",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
    ],
)

cc_binary(
    name = "llvm-mc-embedding",
    srcs = [
        "llvm-mc-embedding/llvm-mc-embedding.cpp",
    ],
    deps = [
        "@//lib:cpp_structures",
        "@//lib:graph",
        "@//lib:target",
        "@//third_party:indicators",
        "@llvm-project//llvm:AllTargetsAsmParsers",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
        "@nlohmann_json//:json",
    ],
)

cc_binary(
    name = "llvm-mc-dataset",
    srcs = [
        "llvm-mc-dataset/llvm-mc-dataset.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
    ],
    deps = [
        "@//lib:cpp_structures",
        "@//lib:statistics",
        "@//third_party:indicators",
        "@llvm-project//llvm:Support",
        "@range-v3",
    ],
)

cc_binary(
    name = "llvm-mc-tokengen",
    srcs = [
        "llvm-mc-tokengen/llvm-mc-tokengen.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
    ],
    deps = [
        "@llvm-project//llvm:AllTargetsAsmParsers",
        "@llvm-project//llvm:AllTargetsDisassemblers",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
    ],
)

pkg_files(
    name = "binary",
    srcs = [
        ":llvm-mc-bench",
        ":llvm-mc-dataset",
        ":llvm-mc-embedding",
        ":llvm-mc-extract",
    ],
    prefix = select(shared_object_path_selector) + "/bin",
    visibility = ["//visibility:public"],
)
