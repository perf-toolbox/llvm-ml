load("//bazel:utils.bzl", "runfiles")
load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
  name = "mc-dataset-report",
  srcs = [
    "dataset-report/main.py"
  ],
  main = "dataset-report/main.py",
  deps = [
    "//lib:py_structures_proto"
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