workspace(name = "llvm-ml")

SKYLIB_VERSION = "1.0.3"

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

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
http_archive(
    name = "com_github_nelhage_rules_boost",

    url = "https://github.com/nelhage/rules_boost/archive/986d23f0fac5e331e54941dfecc1aa3a9a86e543.tar.gz",
    strip_prefix = "rules_boost-986d23f0fac5e331e54941dfecc1aa3a9a86e543",
    sha256 = "4d663a55f42fc16517b37d9af116413606fef0d970441217e9eeba0ab941a61f",
)

# Hedron's Compile Commands Extractor for Bazel
# https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",

    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/800b9cd260ce3878e94abb7d583a7c0865f7d967.tar.gz",
    strip_prefix = "bazel-compile-commands-extractor-800b9cd260ce3878e94abb7d583a7c0865f7d967",
    sha256 = "7f6ebb62298694d8cf3ecaed81b3bb48de559819ac1909d4055abdc8c0ae1000",
)

git_repository(
  name = "nlohmann_json",
  commit = "bbe337c3a30d5f6eea418b4aee399525536de37a",
  remote = "https://github.com/nlohmann/json.git",
  shallow_since = "1678279425 +0100",
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

#load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")
#rules_foreign_cc_dependencies()

# Hack for broken zstd support
new_local_repository(
  name = "llvm_zstd",
  build_file_content="""cc_library(name="zstd", visibility = ["//visibility:public"])""",
  workspace_file_content="",
  path="third_party/zstd_fake"
)

load("//bazel:local_patched_repository.bzl", "local_patched_repository")

local_patched_repository(
    name = "llvm-raw",
    build_file_content = "# empty",
    path = "third_party/llvm-project",
    patches = ["//patches:llvm.patch"],
    patch_args = ["-p1", "--verbose"],
)

load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")

http_archive(
    name = "llvm_zlib",
    build_file = "@llvm-raw//utils/bazel/third_party_build:zlib-ng.BUILD",
    sha256 = "e36bb346c00472a1f9ff2a0a4643e590a254be6379da7cddd9daeb9a7f296731",
    strip_prefix = "zlib-ng-2.0.7",
    url = "https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.0.7.zip",
)

llvm_configure(
  name = "llvm-project",
  # repo_mapping = {"@llvm_zlib": "@zlib"},
  targets = [
        "AArch64",
        "X86",
  ],
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")
hedron_compile_commands_setup()
