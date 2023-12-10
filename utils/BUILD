load("@rules_python//python:defs.bzl", "py_binary")
load("@pip//:requirements.bzl", "requirement")

py_binary(
    name = "mc-dataset-report",
    srcs = [
        "dataset-report/main.py",
    ],
    main = "dataset-report/main.py",
    deps = [
        "//python:llvm_ml",
        requirement("tqdm"),
        requirement("matplotlib"),
        requirement("networkx"),
        requirement("scikit-learn"),
        requirement("numpy"),
    ],
)

py_binary(
    name = "trainer",
    srcs = [
        "nn-trainer/main.py",
    ],
    main = "nn-trainer/main.py",
    deps = [
        "@//python:llvm_ml",
        requirement("torch"),
        requirement("lightning"),
        requirement("torch_geometric"),
    ],
)

py_binary(
    name = "mc-dataset-db",
    srcs = [
        "dataset-db/mc_dataset_db.py",
    ],
    main = "dataset-db/mc_dataset_db.py",
    deps = [
        "@//python:llvm_ml",
        requirement("tqdm"),
        requirement("numpy"),
    ],
)

py_binary(
    name = "mc-llvm-mca-importer",
    srcs = [
        "llvm-mca-importer/main.py",
    ],
    main = "llvm-mca-importer/main.py",
    deps = [
        "@//python:llvm_ml",
        requirement("tqdm"),
        requirement("numpy"),
    ],
)

py_binary(
    name = "ml-bhive-import",
    srcs = [
        "bhive-import/main.py",
    ],
    main = "bhive-import/main.py",
)

cc_binary(
    name = "mc-harness-dump",
    srcs = [
        "mc-harness-dump/main.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
        "//tests:__subpackages__",
    ],
    deps = [
        "@//lib:target",
        "@//tools:llvm-mc-bench-lib",
        "@nlohmann_json//:json",
    ],
)

cc_binary(
    name = "mc-data-gen",
    srcs = [
        "mc-data-gen/main.cpp",
    ],
    visibility = [
        "//tests:__pkg__",
        "//tests:__subpackages__",
    ],
    deps = [
        "@//lib:cpp_structures",
        "@capnp-cpp//src/capnp",
        "@llvm-project//llvm:Support",
    ],
)
