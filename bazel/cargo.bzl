load(
    "@bazel_skylib//lib:paths.bzl",
    "paths",
)
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

_attrs = {
    "path": attr.string(
        mandatory=True,
        doc = "Path to cargo repository",
    ),
    "srcs": attr.label_list(
        mandatory=True,
        doc = "Path to cargo repository",
        allow_files = True,
    ),
    "outs": attr.string_list(
        mandatory=True,
        doc = "List of shared library files",
    ),
    "_cc_toolchain": attr.label(
        default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
    ),
}


def _configure_features(ctx, cc_toolchain):
    disabled_features = ctx.disabled_features
    if not ctx.coverage_instrumented():
        # In coverage mode, cc_common.configure_features() adds coverage related flags,
        # such as --coverage to the compiler and linker. However, if this library is not
        # instrumented, we don't need to pass those flags, and avoid unncessary rebuilds.
        disabled_features.append("coverage")
    return cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = disabled_features,
    )


def _is_debug_mode(ctx):
    return ctx.var.get("COMPILATION_MODE", "fastbuild") == "dbg"


def _cargo_native_libraries_impl(ctx):
    mode = "debug" if _is_debug_mode(ctx) else "release"
    mode_flag = "" if _is_debug_mode(ctx) else "--release"
    bazel_mode = ctx.var.get("COMPILATION_MODE", "fastbuild")

    out_files = []
    touch = ""
    for f in ctx.attr.outs:
        out_path = paths.join(ctx.attr.path, "target", mode, f)
        out_files.append(ctx.actions.declare_file(f))
        touch += "cp " + str(out_path) + " ../bazel-out/k8-" + bazel_mode + "/bin/" + str(ctx.label.package) + "/" + f + " || cp " + str(out_path) + " ../bazel-bin/third_party/" + f + "; "

    ctx.actions.run_shell(
      inputs = ctx.files.srcs,
      outputs = out_files,
      command = "(cd " + paths.join(ctx.label.package, ctx.attr.path) + "; cargo build " + mode_flag + "; cd ../; " + touch + ")",
      use_default_shell_env = True,
      toolchain = Label("@rules_rust//rust:toolchain")
    )

    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = _configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
    )

    linker_files = []
    for f in out_files:
        linker_files.append(cc_common.create_library_to_link(
            actions = ctx.actions,
            dynamic_library = f,
            cc_toolchain = cc_toolchain,
            feature_configuration = feature_configuration,
        ))

    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        libraries = depset(linker_files),
    )

    linking_context = cc_common.create_linking_context(
        linker_inputs = depset([linker_input]),
    )

    return [
        DefaultInfo(runfiles = ctx.runfiles(files = out_files)),
        CcInfo(linking_context = linking_context)
    ]

cargo_native_libraries = rule(
  implementation=_cargo_native_libraries_impl,
  attrs=_attrs,
  output_to_genfiles = False,
  fragments = [
      "apple",
      "cpp",
  ]
)

