workspace(name = "llvm-ml")

SKYLIB_VERSION = "1.0.3"

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

http_archive(
    name = "bazel_skylib",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
)
http_archive(
    name = "rules_foreign_cc",
    strip_prefix = "rules_foreign_cc-0.9.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.9.0.tar.gz",
    sha256 = "2a4d07cd64b0719b39a7c12218a3e507672b82a97b98c6a89d38565894cf7c51",
)

# Hedron's Compile Commands Extractor for Bazel
# https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",

    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/800b9cd260ce3878e94abb7d583a7c0865f7d967.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-800b9cd260ce3878e94abb7d583a7c0865f7d967",
    sha256 = "7f6ebb62298694d8cf3ecaed81b3bb48de559819ac1909d4055abdc8c0ae1000",
)

http_archive(
    name = "rules_rust",
    sha256 = "4a9cb4fda6ccd5b5ec393b2e944822a62e050c7c06f1ea41607f14c4fdec57a2",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.25.1/rules_rust-v0.25.1.tar.gz"],
)

git_repository(
  name = "nlohmann_json",
  commit = "bbe337c3a30d5f6eea418b4aee399525536de37a",
  remote = "https://github.com/nlohmann/json.git",
  shallow_since = "1678279425 +0100",
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

LLVM_COMMIT = "7c1a6c07ccd93a801dad27780cd1301c56c13829"

LLVM_SHA256 = "26dc9f463ed06c7df7a43d738f8b1c0c9eacb2fca8be0f676eb65c7e98a6f18b"

http_archive(
    name = "llvm-raw",
    build_file_content = "# empty",
    sha256 = LLVM_SHA256,
    strip_prefix = "llvm-project-" + LLVM_COMMIT,
    urls = ["https://github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT)],
)

load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")

llvm_configure(
  name = "llvm-project",
  # repo_mapping = {"@llvm_zlib": "@zlib"},
  targets = [
        "AArch64",
        "X86",
  ],
)

maybe(
    http_archive,
    name = "llvm_zlib",
    build_file = "@llvm-raw//utils/bazel/third_party_build:zlib-ng.BUILD",
    sha256 = "e36bb346c00472a1f9ff2a0a4643e590a254be6379da7cddd9daeb9a7f296731",
    strip_prefix = "zlib-ng-2.0.7",
    urls = [
        "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.0.7.zip",
    ],
)

maybe(
    http_archive,
    name = "llvm_zstd",
    build_file = "@llvm-raw//utils/bazel/third_party_build:zstd.BUILD",
    sha256 = "7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0",
    strip_prefix = "zstd-1.5.2",
    urls = [
        "https://github.com/facebook/zstd/releases/download/v1.5.2/zstd-1.5.2.tar.gz",
    ],
)

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()
