load("//bazel:cargo.bzl", "cargo_native_libraries")

cargo_native_libraries(
    name = "libpmu_cargo",
    srcs = glob(
        ["libpmu/**/*"],
        exclude = ["libpmu/target/**/*"],
    ),
    outs = select({
        "@platforms//os:linux": [
            "libpmu.so",
        ],
        "@platforms//os:macos": [
            "libpmu.dylib",
        ],
        "//conditions:default": [],
    }),
    path = "libpmu",
)

cc_library(
    name = "libpmu",
    hdrs = glob(["libpmu/interop/cpp/include/**/*"]),
    strip_include_prefix = "libpmu/interop/cpp/include",
    visibility = ["//tools:__pkg__"],
    deps = [":libpmu_cargo"],
)

cc_library(
    name = "indicators",
    hdrs = [
        "indicators/single_include/indicators/indicators.hpp",
    ],
    strip_include_prefix = "indicators/single_include",
    visibility = ["//tools:__pkg__", "//utils:__pkg__"],
)
