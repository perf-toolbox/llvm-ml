cc_binary(
    name = "llvm-mc-bench",
    srcs = [
      "llvm-mc-bench.cpp",
      "counters.hpp",
      "benchmark.hpp",
      "Target.hpp",
      "Target/X86Target.cpp",
    ] + select({
      "@platforms//os:linux": [
        "counters_linux.cpp",
        "benchmark_linux.cpp"
      ],
      "//conditions:default": [],
    }),
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@llvm-project//llvm:OrcJIT",
    ]
)

