cc_library(
    name = "target",
    srcs = [
      "Target.cpp",
      "X86Target.cpp",
    ],
    hdrs = [
      "Target.hpp"
    ],
    visibility = ["//visibility:public"],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@llvm-project//llvm:X86CommonTableGen",
      "@llvm-project//llvm:X86UtilsAndDesc",
    ]
)
