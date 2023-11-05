_bdist_wheel_attrs = {
    "deps": attr.label_list(
      doc = "List of libraries to include in a wheel",
      mandatory = True,
      allow_empty = False,
    ),
    "data": attr.label_list(
      doc = "List of additional files to be placed outside any package",
      mandatory = False,
    ),
    "strip_src_prefix": attr.string(
      doc = "TODO",
      mandatory = False,
    ),
    "wheel_name": attr.string(),
    "version": attr.string(),
    "description": attr.string(),
    "platform": attr.string_list(
        default = ["any"],
        doc = "Platform the wheel is being built for.",
        mandatory = False,
    ),
    "manifest": attr.string_list(
        default = ["recursive-include {package_name} *"],
        doc = "List of statements to insert into the MANIFEST.in file.",
        mandatory = False,
    ),
    "include_package_data": attr.bool(
        default = False,
        doc = "Whether to use the setuptools `include_package_data` setting. Note that if used with `data`, only data files specified in `manifest` will be included.",
        mandatory = False,
    ),
    "_setup_py_template": attr.label(
        default = Label("//wheel:setup.py.in"),
        allow_single_file = True,
    ),
    "_manifest_template": attr.label(
        default = Label("//wheel:MANIFEST.in"),
        allow_single_file = True,
    ),
}

def _generate_setup_py(ctx):
    classifiers = "[]"
    install_requires = "[]"
    setup_py = ctx.actions.declare_file("{}/setup.py".format(ctx.attr.name))

    data_files = []
    for d in ctx.attr.data:
      for src in d.files.to_list():
        data_files.append(src.basename)

    # create setup.py
    ctx.actions.expand_template(
        template = ctx.file._setup_py_template,
        output = setup_py,
        substitutions = {
            "{name}": ctx.attr.wheel_name,
            "{version}": ctx.attr.version,
            "{description}": ctx.attr.description,
            "{classifiers}": classifiers,
            "{platforms}": str(ctx.attr.platform),
            "{package_data}": "{}",
            "{data_files}": "[('llvm_ml', " + str(data_files) + ")]",
            "{include_package_data}": str(ctx.attr.include_package_data),
            "{install_requires}": install_requires,
        },
        is_executable = True,
    )

    return setup_py

def _generate_manifest(ctx, package_name):
    manifest_text = "\n".join([i for i in ctx.attr.manifest]).format(package_name = package_name)

    if len(ctx.attr.data) > 0:
      for d in ctx.attr.data:
        for f in d.files.to_list():
          manifest_text += "\ninclude " + f.basename

    manifest = ctx.actions.declare_file("{}/MANIFEST.in".format(ctx.attr.name))
    ctx.actions.expand_template(
        template = ctx.file._manifest_template,
        output = manifest,
        substitutions = {
            "{manifest}": manifest_text,
        },
        is_executable = True,
    )

    return manifest

def _remove_prefix(string, prefix):
    if string.startswith(prefix):
        return string[len(prefix):]

def _bdist_wheel_impl(ctx):
    work_dir = "{}/wheel".format(ctx.attr.name)
    build_file_dir = ctx.build_file_path.rstrip("/BUILD")

    package_dir = ctx.actions.declare_directory(work_dir)

    setup_py_dest_dir = package_dir.path

    # FIXME merge sources
    source_list = [src for src in ctx.attr.deps[0].files.to_list()]

    created_dirs = []
    source_tree = []
    source_tree_commands = []

    setup_py = _generate_setup_py(ctx)
    manifest = _generate_manifest(ctx, ctx.attr.wheel_name)

    for src in source_list:
        relative_path = _remove_prefix(src.dirname, ctx.attr.strip_src_prefix).strip("/")
        if relative_path not in created_dirs:
          new_dir = ctx.actions.declare_directory(work_dir + "/" + relative_path)
          source_tree_commands.append("mkdir -p {dir} && chmod 0755 {dir}".format(dir = new_dir.path)) 
          created_dirs.append(relative_path)
          source_tree.append(new_dir)

        new_path = "/".join([
          work_dir,
          _remove_prefix(src.path, ctx.attr.strip_src_prefix).strip("/"),
        ])

        new_file = ctx.actions.declare_file(new_path)
        source_tree.append(new_file)
        source_tree_commands.append("cat {target} > {name}".format(target = src.path, name = new_file.path))

    copied_setup = ctx.actions.declare_file(work_dir + "/setup.py")
    source_tree.append(copied_setup)
    source_tree_commands.append("cp {src} {dst}".format(src = setup_py.path, dst = copied_setup.path))
    copied_manifest = ctx.actions.declare_file(work_dir + "/MANIFEST.in")
    source_tree.append(copied_manifest)
    source_tree_commands.append("cp {src} {dst}".format(src = manifest.path, dst = copied_manifest.path))

    data_inputs = []

    for d in ctx.attr.data:
      for f in d.files.to_list():
        data_file = ctx.actions.declare_file(work_dir + "/" + f.basename)
        source_tree_commands.append("cp {src} {dst}".format(src = f.path, dst = data_file.path))
        source_tree.append(data_file)
        data_inputs.append(f)

    ctx.actions.run_shell(
        mnemonic = "CreateSourceTree",
        outputs = [package_dir] + source_tree,
        inputs = source_list + [setup_py, manifest] + data_inputs,
        command = "mkdir -p {package_dir} && chmod 0755 {package_dir} && {cmd}".format(package_dir = package_dir.path, cmd = " && ".join(source_tree_commands)),
    )

    command = "export ROOT=$(pwd)/{dist_dir} && cd {package_dir} " + \
              "&& python setup.py bdist_wheel --universal --dist-dir=$ROOT "
    ctx.actions.run_shell(
        mnemonic = "BuildWheel",
        outputs = [ctx.outputs.wheel],
        inputs = source_tree,
        command = command.format(
            package_dir = setup_py_dest_dir,
            dist_dir = ctx.outputs.wheel.dirname,
        ),
    )

    return DefaultInfo(files = depset([ctx.outputs.wheel]))

_bdist_wheel_outputs = {
    "wheel": "%{wheel_name}-%{version}-py2.py3-none-%{platform}.whl",
}

bdist_wheel = rule(
    doc = """"\
TODO write docs
    """,
    implementation = _bdist_wheel_impl,
    attrs = _bdist_wheel_attrs,
    outputs = _bdist_wheel_outputs,
)
