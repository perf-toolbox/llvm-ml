load("@rules_python//python:defs.bzl", "py_binary")
load("@python_versions//3.10:defs.bzl", compile_pip_requirements_3_10 = "compile_pip_requirements")
load("@pip//:requirements.bzl", "requirement")

compile_pip_requirements_3_10(
    name = "requirements_3_10",
    extra_args = ["--allow-unsafe"],
    requirements_in = "requirements.in",
    requirements_txt = "requirements_lock_3_10.txt",
    requirements_windows = "requirements_windows_3_10.txt",
)

py_binary(
  name = "mc-dataset-report",
  srcs = [
    "dataset-report/main.py"
  ],
  main = "dataset-report/main.py",
  deps = [
    "@llvm-ml-py//:llvm_ml",
    requirement("tqdm"),
    requirement("matplotlib"),
    requirement("networkx"),
    requirement("scikit-learn"),
    requirement("numpy"),
  ]
)

py_binary(
  name = "mc-dataset-db",
  srcs = [
    "dataset-db/mc_dataset_db.py"
  ],
  main = "dataset-db/mc_dataset_db.py",
  deps = [
    "@llvm-ml-py//:llvm_ml",
    requirement("tqdm"),
    requirement("numpy"),
  ]
)

py_binary(
  name = "mc-llvm-mca-importer",
  srcs = [
    "llvm-mca-importer/main.py"
  ],
  main = "llvm-mca-importer/main.py",
  deps = [
    "@llvm-ml-py//:llvm_ml",
    requirement("tqdm"),
    requirement("numpy"),
  ]
)

py_binary(
  name = "ml-bhive-import",
  srcs = [
    "bhive-import/main.py"
  ],
  main = "bhive-import/main.py",
)

cc_binary(
    name = "mc-harness-dump",
    srcs = [
      "mc-harness-dump/main.cpp",
    ],
    deps = [
      "@//tools:llvm-mc-bench-lib",
      "@nlohmann_json//:json",
      "@llvm-ml-libs//:target",
    ],
    visibility = [
      "//tests:__pkg__",
      "//tests:__subpackages__",
    ]
)

cc_binary(
    name = "mc-data-gen",
    srcs = [
      "mc-data-gen/main.cpp",
    ],
    deps = [
      "@llvm-ml-libs//:cpp_structures",
      "@llvm-project//llvm:Support",
      "@capnp-cpp//src/capnp",
    ],
    visibility = [
      "//tests:__pkg__",
      "//tests:__subpackages__",
    ]
)
