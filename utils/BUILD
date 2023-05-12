load("@rules_python//python:defs.bzl", "py_binary")
#load("@pip//:requirements.bzl", "requirement")

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