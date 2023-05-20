cc_library(
  name = "indicators",
  hdrs = [
    "indicators/single_include/indicators/indicators.hpp"
  ],
  strip_include_prefix = "indicators/single_include",
  visibility = ["//tools:__pkg__"],
)

cc_library(
  name = "libpfm",
  hdrs = glob(["libpfm4/include/**/*.h"]),
  srcs = glob([
      "libpfm4/lib/**/pfmlib_intel*.c",
      "libpfm4/lib/**/pfmlib_amd64*.c",
    ],
    exclude = [
      "libpfm4/lib/**/*_perf_event.c"
    ]) + glob(["libpfm4/lib/**/*.h"]) + glob(["libpfm4/include/**/*.h"]) +
    ["libpfm4/lib/pfmlib_common.c"] + select({
      "@platforms//os:linux": [
        "libpfm4/lib/pfmlib_perf_event_pmu.c",
        "libpfm4/lib/pfmlib_perf_event.c",
        "libpfm4/lib/pfmlib_perf_event_raw.c",
      ] + glob([
        "libpfm4/lib/pfmlib_intel*_perf_event.c",
        "libpfm4/lib/pfmlib_amd64*_perf_event.c",
      ]),
      "//conditions:default": [],
    }),
  defines = [
    "CONFIG_PFMLIB_ARCH_X86",
    "CONFIG_PFMLIB_ARCH_I386",
    "CONFIG_PFMLIB_ARCH_X86_64",
    "_REENTRANT",
  ] + select({
    "@platforms//os:linux": [
      "HAS_OPENAT",
      "CONFIG_PFMLIB_OS_LINUX",
    ],
    "//conditions:default": [],
  }),
  copts = select({
    "@platforms//os:linux": [
      "-fvisibility=hidden",
    ],
    "//conditions:default": [],
  }),
  linkstatic = True,
  strip_include_prefix = "libpfm4/include",
  visibility = ["//tools:__pkg__"],
)
