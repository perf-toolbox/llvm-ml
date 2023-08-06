load("@rules_python//python:packaging.bzl", "py_package", "py_wheel")
load("@python_versions//3.10:defs.bzl", compile_pip_requirements_3_10 = "compile_pip_requirements")
load("@rules_python//python:defs.bzl", "py_library")
load("@llvm_ml_py_pip//:requirements.bzl", "requirement")

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
  deps = [
    requirement("numpy"),
    requirement("tqdm"),
    requirement("protobuf"),
  ],
  data = [
    "@llvm-ml-libs//:py_structures",
    ":_llvm_ml_impl.py"
  ],
  visibility = ["//visibility:public"],
)

py_package(
  name = "llvm_ml_pkg",
  deps = [
    "//:llvm_ml",
  ],
  visibility = ["//visibility:public"]
)

