load("@bazel_skylib//lib:paths.bzl", "paths")

def resolve_path(label):
    """Returns the path to the package of 'label'.

    Args:
      label: label. The label to return the package path of.

    For example, package_path("@foo//bar:BUILD") returns 'external/foo/bar'.
    """
    return paths.join(Label(label).workspace_root, Label(label).package)
