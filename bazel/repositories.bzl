load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def init_repositories():
    RANGES_COMMIT = "9aa41d6b8ded2cf5e8007e66a0efd1ab33dbf9a5"

    http_archive(
        name = "range-v3",
        urls = ["https://github.com/ericniebler/range-v3/archive/{commit}.tar.gz".format(commit = RANGES_COMMIT)],
        strip_prefix = "range-v3-" + RANGES_COMMIT,
        sha256 = "94013ebb376684769e378c3989fd3b9ec01c2c11d411ccb7b9eb13ad8decb4e3",
    )
