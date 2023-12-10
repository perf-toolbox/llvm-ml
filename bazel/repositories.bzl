load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def init_repositories():
    RANGES_COMMIT = "9aa41d6b8ded2cf5e8007e66a0efd1ab33dbf9a5"

    http_archive(
        name = "range-v3",
        urls = ["https://github.com/ericniebler/range-v3/archive/{commit}.tar.gz".format(commit = RANGES_COMMIT)],
        strip_prefix = "range-v3-" + RANGES_COMMIT,
        sha256 = "94013ebb376684769e378c3989fd3b9ec01c2c11d411ccb7b9eb13ad8decb4e3",
    )

    CAPNPROTO_TAG = "c0bebbc38913951b75ed05ac38117d8c9c3001b2"

    http_archive(
        name = "capnp-cpp",
        urls = ["https://github.com/capnproto/capnproto/archive/{tag}.tar.gz".format(tag = CAPNPROTO_TAG)],
        strip_prefix = "capnproto-{tag}/c++".format(tag = CAPNPROTO_TAG),
        sha256 = "71112f60d1d0eb31c5e5b7fa3b80f9680c0cd961ca93ea88d07bddf8dd8b72fe",
    )

    http_archive(
        name = "nanobind",
        urls = ["https://github.com/wjakob/nanobind/archive/refs/tags/v1.2.0.tar.gz"],
        strip_prefix = "nanobind-1.2.0",
        workspace_file_content = "# empty",
        sha256 = "ce6a23a7b1a7b70d2f3f55c79975d2cf2d94dcae15b7a0dc5b2f96521a6fb40e",
        build_file = "//python:nanobind.BUILD",
    )
