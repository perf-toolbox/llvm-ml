load("//:repositories.bzl", "load_dependencies")

def _non_module_dependencies_impl(_ctx):
    load_dependencies()

non_module_dependencies = module_extension(
    implementation = _non_module_dependencies_impl,
)
