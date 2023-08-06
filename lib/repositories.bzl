load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def load_dependencies():
    http_archive(
        name = "bazel_skylib",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        ],
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    )

    CAPNPROTO_TAG = "c0bebbc38913951b75ed05ac38117d8c9c3001b2"

    http_archive(
        name = "capnp-cpp",
        urls = ["https://github.com/capnproto/capnproto/archive/{tag}.tar.gz".format(tag = CAPNPROTO_TAG)],
        strip_prefix = "capnproto-{tag}/c++".format(tag = CAPNPROTO_TAG),
        sha256 = "71112f60d1d0eb31c5e5b7fa3b80f9680c0cd961ca93ea88d07bddf8dd8b72fe"
    )

    http_archive(
      name = "nanobind",
      urls = ["https://github.com/wjakob/nanobind/archive/refs/tags/v1.2.0.tar.gz"],
      strip_prefix = "nanobind-1.2.0",
      workspace_file_content = "# empty",
      sha256 = "ce6a23a7b1a7b70d2f3f55c79975d2cf2d94dcae15b7a0dc5b2f96521a6fb40e",
      build_file = "//:nanobind.BUILD",
    )
