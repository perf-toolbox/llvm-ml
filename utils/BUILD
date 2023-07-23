load("//bazel:utils.bzl", "runfiles")
load("@rules_python//python:defs.bzl", "py_binary")
load("@python_versions//3.10:defs.bzl", compile_pip_requirements_3_10 = "compile_pip_requirements")
load("@pip//:requirements.bzl", "requirement")

# This stanza calls a rule that generates targets for managing pip dependencies
# with pip-compile.
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
    "//lib:py_structures_proto",
    requirement("tqdm"),
    requirement("matplotlib"),
    requirement("networkx"),
    requirement("scikit-learn"),
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

runfiles(
  name = "llvm-mc",
  deps = ["@llvm-project//llvm:llvm-mc"]
)

cc_binary(
    name = "mc-harness-dump",
    srcs = [
      "mc-harness-dump/main.cpp",
    ],
    deps = [
      "@//tools:llvm-mc-bench-lib",
      "@nlohmann_json//:json",
      "@//lib:target",
      "@//lib:cpp_structures_proto",
    ] + select({
      "@platforms//os:linux": [
        "//third_party:libpfm",
      ],
      "//conditions:default": [],
    }),
    visibility = [
      "//tests:__pkg__",
      "//tests:__subpackages__",
    ]
)
