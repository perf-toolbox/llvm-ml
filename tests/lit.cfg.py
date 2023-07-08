# -*- Python -*-

# Configuration file for the 'lit' test runner.

import os
import sys
import re
import platform
import subprocess

import lit.util
import lit.formats
from lit.llvm import llvm_config
from lit.llvm.subst import FindTool
from lit.llvm.subst import ToolSubst

# name: The name of this test suite.
config.name = 'llvm-ml'

# testFormat: The test format to use to interpret tests.
config.test_format = lit.formats.ShTest(not llvm_config.use_lit_shell)

# suffixes: A list of file extensions to treat as test files. This is overriden
# by individual lit.local.cfg files in the test subdirectories.
config.suffixes = ['.s']

# excludes: A list of directories to exclude from the testsuite. The 'Inputs'
# subdirectories contain auxiliary inputs for various tests in their parent
# directories.
config.excludes = ['Inputs', 'CMakeLists.txt', 'README.txt', 'LICENSE.txt']

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.llvm_ml_obj_root, 'tests')

# Tweak the PATH to include the tools dir.
llvm_config.with_environment('PATH', config.llvm_tools_dir, append_path=True)

# Propagate some variables from the host environment.
llvm_config.with_system_environment(
    ['HOME', 'INCLUDE', 'LIB', 'TMP', 'TEMP'])

llvm_config.use_default_substitutions()

# Add site-specific substitutions.
config.substitutions.append(('%llvmshlibdir', config.llvm_shlib_dir))
config.substitutions.append(('%shlibext', config.llvm_shlib_ext))
config.substitutions.append(('%pluginext', config.llvm_plugin_ext))
config.substitutions.append(('%exeext', config.llvm_exe_ext))

llvm_tools = [
    ToolSubst('%llvm', FindTool('llvm'), unresolved='ignore'),
]

utils = [
    ToolSubst('%mc-harness-dump', FindTool('mc-harness-dump')),
]

llvm_config.add_tool_substitutions(llvm_tools, config.llvm_tools_dir)
llvm_config.add_tool_substitutions(utils, config.llvm_ml_utils_root)

# Targets

config.targets = frozenset(config.targets_to_build.split())

for arch in config.targets_to_build.split():
    config.available_features.add(arch.lower() + '-registered-target')

# Features
known_arches = ["x86_64", "mips64", "ppc64", "aarch64"]
if (config.host_ldflags.find("-m32") < 0
    and any(config.llvm_host_triple.startswith(x) for x in known_arches)):
  config.available_features.add("llvm-64-bits")

config.available_features.add("host-byteorder-" + sys.byteorder + "-endian")

if sys.platform in ['win32']:
    # ExecutionEngine, no weak symbols in COFF.
    config.available_features.add('uses_COFF')
else:
    # Others/can-execute.txt
    config.available_features.add('can-execute')

# LLVM can be configured with an empty default triple
# Some tests are "generic" and require a valid default triple
if config.target_triple:
    config.available_features.add('default_triple')
    # Direct object generation
    if not config.target_triple.startswith(("nvptx", "xcore")):
        config.available_features.add('object-emission')

import subprocess


# .debug_frame is not emitted for targeting Windows x64, aarch64/arm64, AIX, or Apple Silicon Mac.
if not re.match(r'^(x86_64|aarch64|arm64|powerpc|powerpc64).*-(windows-gnu|windows-msvc|aix)', config.target_triple) \
    and not re.match(r'^arm64(e)?-apple-(macos|darwin)', config.target_triple):
    config.available_features.add('debug_frame')
