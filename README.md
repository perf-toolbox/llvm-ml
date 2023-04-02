# LLVM ML: Machine Learning tools for LLVM
LLVM ML is a set of tools that makes it easier to apply machine learning
techniques to compiler development. This project aims to provide useful tools
that enable developers to extract features from LLVM-based compilers and use
them in machine learning models.

## Introduction

LLVM is a collection of modular and reusable compiler and toolchain technologies
that can be used to develop a wide range of compilers and related tools. LLVM ML
is a set of tools that makes it easier to apply machine learning techniques to
LLVM-based compiler development.

## Tools

### llvm-mc-embedding

`llvm-mc-embedding` is a tool that takes an assembly source as an input
and converts it to a graph embedding. This tool can be used to extract
features from compilation result and use them in machine learning
algorithms. Here's how to use it:

```sh
./bazel-bin/llvm-mc-embedding/llvm-mc-embedding -o out.txt input.s
```

### llvm-mc-extract

`llvm-mc-extract` is a tool that takes a binary file as an input
and produces a bunch of assembly files, that contain basic blocks.
This tool can be used to obtain a dataset of real-world assembly
code for further analysis or use with machine learning algorithms.
Here's how to use it:

```sh
./bazel-bin/llvm-mc-extract/llvm-mc-extract -o /path/to/out/dir /path/to/bin
```

## Dependencies

LLVM ML requires the following dependencies:

- Bazel 6.0
- LLVM top of the trunk

## Build

To build the project, follow these steps:

```sh
git clone --recursive https://github.com/perf-toolbox/llvm-ml.git
cd llvm-ml
bazel build //...
```

For local development you may want to create a file named `user.bazelrc` with the following content:

```starlark
build --disk_cache=~/.cache/llvm-ml
```

## License

LLVM ML is released under the Apache License, Version 2.0. See LICENSE for more information.
