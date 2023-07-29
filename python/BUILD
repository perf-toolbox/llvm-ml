load("@rules_python//python:packaging.bzl", "py_package", "py_wheel")
load("@python_versions//3.10:defs.bzl", compile_pip_requirements_3_10 = "compile_pip_requirements")
load("@rules_python//python:defs.bzl", "py_library")
load("@pip//:requirements.bzl", "requirement")

compile_pip_requirements_3_10(
    name = "requirements_3_10",
    extra_args = ["--allow-unsafe"],
    requirements_in = "requirements.in",
    requirements_txt = "requirements_lock_3_10.txt",
    requirements_windows = "requirements_windows_3_10.txt",
)

py_library(
  name = "llvm_ml",
  srcs = glob(["llvm_ml/**/*.py"], allow_empty=False),
  imports = ["."],
  deps = [
    "//lib:py_structures_proto",
    requirement("numpy"),
    requirement("tqdm"),
    requirement("protobuf"),
  ],
  visibility = ["//utils:__pkg__"],
)

py_package(
  name = "llvm_ml_pkg",
  deps = [
    "//python:llvm_ml",
  ],
)

py_wheel(
  name = "llvm_ml_wheel",
  distribution = "llvm_ml",
  deps = [
    ":llvm_ml_pkg",
  ],
  python_tag = "py3",
  version = "0.0.1",
)
