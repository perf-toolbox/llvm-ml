load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")

def _init_llvm_impl(_ctx):
    llvm_configure(
        name = "llvm-project",
        targets = [
            "AArch64",
            "X86",
        ],
    )

init_llvm = module_extension(
    implementation = _init_llvm_impl,
)
