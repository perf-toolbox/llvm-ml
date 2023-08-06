load("@capnp-cpp//src/capnp:cc_capnp_library.bzl", "cc_capnp_library")

cc_capnp_library(
  name = "structures_gen",
  srcs = [
    "structures/mc_dataset.capnp",
    "structures/mc_metrics.capnp",
    "structures/mc_graph.capnp",
  ],
)

cc_library(
    name = "cpp_structures",
    hdrs = [
        "structures/mc_dataset.capnp.h",
        "structures/mc_graph.capnp.h",
        "structures/mc_metrics.capnp.h",
        "structures/structures.hpp",
    ],
    srcs = [
        "structures/mc_dataset.capnp.c++",
        "structures/mc_metrics.capnp.c++",
        "structures/mc_graph.capnp.c++",
        "structures/structures.cpp",
    ],
    include_prefix = "llvm-ml",
    deps = [":structures_gen"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "py_structures_static",
    srcs = [
        "structures/py_structures.cpp",
    ],
    deps = [
        ":cpp_structures",
        "@nanobind",
    ],
    linkstatic = True,
)

cc_shared_library(
    name = "py_structures",
    deps = [
        ":py_structures_static",
    ],
    shared_lib_name = select({
      "@platforms//os:linux": "_llvm_ml_impl.so",
      "@platforms//os:macos": "_llvm_ml_impl.dylib",
      "//conditions:default": "",
    }),
    visibility = ["//visibility:public"]
)

