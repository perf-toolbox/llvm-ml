load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
load("@python_versions//3.11:defs.bzl", py_binary_11 = "py_binary")
load("@rules_python//python/pip_install:requirements.bzl", "compile_pip_requirements")
load("@rules_python//python:packaging.bzl", "py_wheel")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files", "pkg_mkdirs", "strip_prefix")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//bazel/testing:fake_py_test.bzl", "fake_py_test")
load("@rules_wheel//wheel:rules.bzl", "bdist_wheel")

compile_pip_requirements(
    name = "requirements_3_11",
    py_binary = py_binary_11,
    py_test = fake_py_test,
    extra_args = ["--allow-unsafe"],
    requirements_in = "requirements.in",
    requirements_txt = "requirements_lock_3_11.txt",
    requirements_windows = "requirements_windows_3_11.txt",
)

bdist_wheel(
  name = "llvm_ml_wheel",
  wheel_name = "llvm_ml",
  deps = [
    "//python:llvm_ml",
  ],
  data = [
    "//python:py_structures"
  ],
  include_package_data = True,
  version = "0.0.1",
  strip_src_prefix = "python"
)

#py_wheel(
#    name = "llvm_ml_wheel",
#    distribution = "llvm_ml",
#    deps = [
#        "@llvm-ml-py//:llvm_ml_pkg",
#    ],
#    python_tag = "py3",
#    version = "0.0.1",
#)

pkg_filegroup(
    name = "tools",
    srcs = [
        "//tools:binary",
    ]
)

pkg_tar(
    name = "llvm_ml_tools",
    srcs = [
        ":tools",
    ],
    compressor = "@llvm_zstd//:zstd-cli",
    extension = "tar.zst",
    tags = ["manual"],
)

refresh_compile_commands(
    name = "refresh_compile_commands",
    exclude_external_sources = True,
    exclude_headers = "external"
)
