def _impl(ctx):
  for dep in ctx.attr.deps:
    print(dep.files_to_run.executable.path)

runfiles = rule(
  implementation = _impl,
  attrs = {
    "deps": attr.label_list(allow_files=True),
  }
)
