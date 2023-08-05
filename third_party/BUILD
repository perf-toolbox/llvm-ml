load("//bazel:cargo.bzl", "cargo_native_libraries")

cargo_native_libraries(
  name = "libpmu_cargo",
  path = "libpmu",
  srcs = glob(["libpmu/**/*"], exclude=["libpmu/target/**/*"]),
  outs = select({
      "@platforms//os:linux": [
        "libpmu.so",
      ],
      "@platforms//os:macos": [
        "libpmu.dylib"
      ],
      "//conditions:default": [],
    })
)

cc_library(
  name = "libpmu",
  hdrs = glob(["libpmu/interop/cpp/include/**/*"]),
  deps = [":libpmu_cargo"],
  visibility = ["//tools:__pkg__"],
  strip_include_prefix = "libpmu/interop/cpp/include",
)

cc_library(
  name = "indicators",
  hdrs = [
    "indicators/single_include/indicators/indicators.hpp"
  ],
  strip_include_prefix = "indicators/single_include",
  visibility = ["//tools:__pkg__"],
)
