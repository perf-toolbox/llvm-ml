load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def init_repositories():
    LLVM_COMMIT = "7c1a6c07ccd93a801dad27780cd1301c56c13829"

    LLVM_SHA256 = "26dc9f463ed06c7df7a43d738f8b1c0c9eacb2fca8be0f676eb65c7e98a6f18b"

    http_archive(
        name = "llvm-raw",
        build_file_content = "# empty",
        sha256 = LLVM_SHA256,
        strip_prefix = "llvm-project-" + LLVM_COMMIT,
        urls = ["https://github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT)],
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
        build_file = "//third_party:zstd.BUILD",
        sha256 = "7c42d56fac126929a6a85dbc73ff1db2411d04f104fae9bdea51305663a83fd0",
        strip_prefix = "zstd-1.5.2",
        urls = [
            "https://github.com/facebook/zstd/releases/download/v1.5.2/zstd-1.5.2.tar.gz",
        ],
    )

    RANGES_COMMIT = "9aa41d6b8ded2cf5e8007e66a0efd1ab33dbf9a5"

    http_archive(
        name = "range-v3",
        urls = ["https://github.com/ericniebler/range-v3/archive/{commit}.tar.gz".format(commit = RANGES_COMMIT)],
        strip_prefix = "range-v3-" + RANGES_COMMIT,
        sha256 = "94013ebb376684769e378c3989fd3b9ec01c2c11d411ccb7b9eb13ad8decb4e3",
    )
