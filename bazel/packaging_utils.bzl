shared_object_path_selector = {
    "@platforms//os:linux": "/usr/share/llvm-ml",
    "@platforms//os:macos": "/Library/llvm-ml",
    "@platforms//os:windows": "/Program Files/llvm-ml",
    "//conditions:default": "/usr/local/share/llvm-ml",
}
