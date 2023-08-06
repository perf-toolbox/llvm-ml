# This file is licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

package(
    default_visibility = ["//visibility:public"],
    # BSD/MIT-like license (for zstd)
    licenses = ["notice"],
)

bool_flag(
    name = "llvm_enable_zstd",
    build_setting_default = True,
)

config_setting(
    name = "llvm_zstd_enabled",
    flag_values = {":llvm_enable_zstd": "true"},
)

cc_library(
    name = "zstd",
    srcs = glob([
        "lib/common/*.c",
        "lib/common/*.h",
        "lib/compress/*.c",
        "lib/compress/*.h",
        "lib/decompress/*.c",
        "lib/decompress/*.h",
        "lib/decompress/*.S",
        "lib/dictBuilder/*.c",
        "lib/dictBuilder/*.h",
    ]),
    hdrs = [
        "lib/zstd.h",
        "lib/zdict.h",
        "lib/zstd_errors.h",
    ],
    defines = select({
        ":llvm_zstd_enabled": [
            "LLVM_ENABLE_ZSTD=1",
        ],
        "//conditions:default": [
            "ZSTD_MULTITHREAD",
        ],
    }),
    strip_include_prefix = "lib",
)

cc_library(
    name='datagen',
    hdrs=['programs/datagen.h'],
    strip_include_prefix = "programs",
    srcs=['programs/datagen.c'],
    deps=['//:zstd', ":util"],
)

cc_library(
    name='util',
    hdrs=['programs/util.h', 'programs/platform.h'],
    strip_include_prefix = "programs",
    deps=['//:zstd'],
)

cc_binary(
  name = "zstd-cli",
  srcs = glob(['programs/*.h', 'programs/*.c'], exclude=['programs/datagen.h', 'programs/platform.h', 'programs/util.h', 'programs/datagen.c']),
  deps = [
    ":zstd",
    ":util",
    ":datagen",
  ]
)
