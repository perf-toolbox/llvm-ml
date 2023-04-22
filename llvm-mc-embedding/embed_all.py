#!/usr/bin/env python3

import sys
import os
import subprocess

path = sys.argv[1]
out = sys.argv[2]

if not os.path.isdir(path):
    raise Exception("Not a directory")

blocks = [each for each in os.listdir(path) if each.endswith('.s')]

for b in blocks:
    try:
        print("Embeddings for {}".format(b))
        res = subprocess.run(["./bazel-bin/llvm-mc-embedding/llvm-mc-embedding", os.path.join(path, b), "-o", os.path.join(out, b + ".json")], check=True, capture_output=True)
        print(res.stdout)
        print(res.stderr)
    except subprocess.CalledProcessError as err:
        print("Failed to generate")
        print(err)
