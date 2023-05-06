load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:proto.bzl", "py_proto_library")

proto_library(
    name = "structures_proto",
    srcs = [
        "structures/bb_graph.proto",
        "structures/bb_metrics.proto",
        "structures/mc_dataset.proto",
    ]
)

cc_proto_library(
    name = "cpp_structures_proto",
    deps = [":structures_proto"],
    visibility = ["//visibility:public"],
)

py_proto_library(
    name = "py_structures_proto",
    deps = [":structures_proto"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "target",
    srcs = [
       "target/Target.cpp",
        "target/X86Target.cpp",
    ],
    hdrs = [
        "target/Target.hpp"
    ],
    include_prefix = "llvm-ml",
    visibility = ["//tools:__pkg__"],
    deps = [
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Target",
        "@llvm-project//llvm:X86CommonTableGen",
        "@llvm-project//llvm:X86UtilsAndDesc",
    ]
)

