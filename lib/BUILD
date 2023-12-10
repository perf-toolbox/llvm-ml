load("@capnp-cpp//src/capnp:cc_capnp_library.bzl", "cc_capnp_library")

cc_capnp_library(
    name = "structures_gen",
    srcs = [
        "structures/mc_dataset.capnp",
        "structures/mc_graph.capnp",
        "structures/mc_metrics.capnp",
    ],
)

cc_library(
    name = "cpp_structures",
    srcs = [
        "structures/mc_dataset.capnp.c++",
        "structures/mc_graph.capnp.c++",
        "structures/mc_metrics.capnp.c++",
        "structures/structures.cpp",
    ],
    hdrs = [
        "structures/mc_dataset.capnp.h",
        "structures/mc_graph.capnp.h",
        "structures/mc_metrics.capnp.h",
        "structures/structures.hpp",
    ],
    include_prefix = "llvm-ml",
    visibility = ["//visibility:public"],
    deps = [":structures_gen"],
)

cc_library(
    name = "target",
    srcs = [
        "target/Target.cpp",
        "target/X86Target.cpp",
    ],
    hdrs = [
        "target/Target.hpp",
    ],
    include_prefix = "llvm-ml",
    visibility = ["//visibility:public"],
    deps = [
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:MCDisassembler",
        "@llvm-project//llvm:MCParser",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
        "@llvm-project//llvm:X86CommonTableGen",
        "@llvm-project//llvm:X86UtilsAndDesc",
    ],
)

cc_library(
    name = "graph",
    srcs = [
        "graph/Graph.cpp",
    ],
    hdrs = [
        "graph/Graph.hpp",
    ],
    include_prefix = "llvm-ml",
    visibility = ["//visibility:public"],
    deps = [
        ":target",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
    ],
)

cc_library(
    name = "statistics",
    hdrs = [
        "statistics/cov.hpp",
    ],
    include_prefix = "llvm-ml",
    visibility = ["//visibility:public"],
)

cc_test(
    name = "statistics_test",
    srcs = [
        "unittests/cov.cpp",
    ],
    deps = [
        ":statistics",
        "@catch2",
        "@catch2//:catch2_main",
    ],
)
