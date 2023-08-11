load("@hedron_compile_commands//:refresh_compile_commands.bzl", "refresh_compile_commands")
load("@rules_python//python:packaging.bzl", "py_wheel")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files", "pkg_mkdirs", "strip_prefix")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

py_wheel(
    name = "llvm_ml_wheel",
    distribution = "llvm_ml",
    deps = [
        "@llvm-ml-py//:llvm_ml_pkg",
    ],
    python_tag = "py3",
    version = "0.0.1",
)

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
)

refresh_compile_commands(
    name = "refresh_compile_commands",
    exclude_external_sources = True,
    exclude_headers = "external"
)