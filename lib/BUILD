cc_library(
    name = "target",
    srcs = [
      "target/Target.cpp",
      "target/X86Target.cpp",
    ],
    hdrs = [
      "target/Target.hpp"
    ],
    #strip_include_prefix = "lib",
    include_prefix = "llvm-ml",
    visibility = ["//visibility:public"],
    deps = [
      "@llvm-project//llvm:Support",
      "@llvm-project//llvm:MC",
      "@llvm-project//llvm:Target",
      "@llvm-project//llvm:X86CommonTableGen",
      "@llvm-project//llvm:X86UtilsAndDesc",
    ]
)

