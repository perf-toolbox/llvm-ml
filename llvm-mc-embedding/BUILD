cc_binary(
    name = "llvm-mc-embedding",
    srcs = [
      "llvm-mc-embedding.cpp",
    ],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:AllTargetsAsmParsers",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@boost//:graph"
    ]
)
