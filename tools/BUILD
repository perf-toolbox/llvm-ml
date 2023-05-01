cc_binary(
    name = "llvm-mc-bench",
    srcs = [
      "llvm-mc-bench/llvm-mc-bench.cpp",
      "llvm-mc-bench/counters.hpp",
      "llvm-mc-bench/benchmark.hpp",
    ] + select({
      "@platforms//os:linux": [
        "llvm-mc-bench/counters_linux.cpp",
        "llvm-mc-bench/benchmark_linux.cpp"
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
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@//lib:target",
    ]
)

cc_binary(
    name = "llvm-mc-embedding",
    srcs = [
      "llvm-mc-embedding/llvm-mc-embedding.cpp",
    ],
    defines = [
      "BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION=1",
      "BOOST_THREAD_PROVIDES_EXECUTORS",
      "BOOST_THREAD_PROVIDES_FUTURE",
      "BOOST_THREAD_USES_MOVE",
    ],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@boost//:graph",
      "@boost//:thread",
      "@nlohmann_json//:json",
      "@//lib:target",
      "@//lib:cpp_structures_proto",
      "@//third_party:indicators",
    ]
)
