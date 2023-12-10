load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def load_dependencies():
    http_archive(
        name = "nanobind",
        urls = ["https://github.com/wjakob/nanobind/archive/refs/tags/v1.2.0.tar.gz"],
        strip_prefix = "nanobind-1.2.0",
        workspace_file_content = "# empty",
        sha256 = "ce6a23a7b1a7b70d2f3f55c79975d2cf2d94dcae15b7a0dc5b2f96521a6fb40e",
        build_file = "//:nanobind.BUILD",
    )
