config_setting(
    name = "msvc_compiler",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "msvc-cl"},
)

config_setting(
    name = "macos",
    constraint_values = ["@platforms//os:macos"],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "linux",
    constraint_values = ["@platforms//os:macos"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "nanobind",
    hdrs = glob(
        include = [
            "include/nanobind/*.h",
            "include/nanobind/stl/*.h",
            "include/nanobind/stl/detail/*.h",
        ],
    ),
    srcs = [
        "include/nanobind/stl/detail/nb_dict.h",
        "include/nanobind/stl/detail/nb_list.h",
        "include/nanobind/stl/detail/traits.h",
        "src/buffer.h",
        "src/common.cpp",
        "src/error.cpp",
        "src/implicit.cpp",
        "src/nb_enum.cpp",
        "src/nb_func.cpp",
        "src/nb_internals.cpp",
        "src/nb_internals.h",
        "src/nb_ndarray.cpp",
        "src/nb_type.cpp",
        "src/trampoline.cpp",
    ],
    copts = select({
        ":msvc_compiler": ["/std:c++17"],
        "//conditions:default": [
            "--std=c++17",
            "-fexceptions",
            "-fno-strict-aliasing",
        ],
    }) + select({
        ":linux": [
            "-ffunction-sections",
            "-fdata-sections",
        ],
        "//conditions:default": []
    }),
    linkopts = select({
        ":macos": [
            "-undefined dynamic_lookup",
            "-Wl,-no_fixup_chains",
            "-Wl,-dead_strip",
        ],
        "//conditions:default": [],
    }),
    linkstatic = True,
    includes = ["include"],
    strip_include_prefix = "include",
    deps = [
        "@python_3_10//:python_headers",
        "@robin-map//:robin-map",
    ],
    local_defines = ["NB_BUILD", "NB_SHARED"],
    visibility = ["//visibility:public"],
)
