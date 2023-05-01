#!/usr/bin/env python3

import sys
import os
import subprocess
import argparse
import multiprocessing
import concurrent.futures

parser = argparse.ArgumentParser(
            prog="Basic block to graph embeddings converter"
        )
parser.add_argument("input")
parser.add_argument("output")
parser.add_argument("--jobs", default=multiprocessing.cpu_count(),  type=int) 
parser.add_argument("--triple", type=str) 
parser.add_argument("--readable-json", action="store_true")
parser.add_argument("--dot", action="store_true")
parser.add_argument("--virtual-root", action="store_true")
parser.add_argument("--in-order", action="store_true")
parser.add_argument("--progress", action="store_true")
parser.add_argument("--verbose", action="store_true")

args = parser.parse_args()

path = args.input
out = args.output

if not os.path.isdir(path):
    raise Exception("Not a directory")

blocks = [each for each in os.listdir(path) if each.endswith('.s')]

def process_block(input):
    try:
        tool_args = ["./bazel-bin/llvm-mc-embedding/llvm-mc-embedding"]

        out_filename = input.replace(".s", ".json")

        if not args.triple is None:
            tool_args.append("--triple")
            tool_args.append(args.triple)
        if args.dot:
            out_filename = out_filename.replace(".json", ".dot")
            tool_args.append("--dot")
        elif args.readable_json:
            tool_args.append("--readable-json")
        if args.in_order:
            tool_args.append("--in-order")

        tool_args.append(os.path.join(path, input))
        tool_args.append("-o")
        tool_args.append(os.path.join(out, out_filename))
        res = subprocess.run(tool_args, check=True, capture_output=True)
        if args.verbose:
            print(res.stdout)
            print(res.stderr)
    except subprocess.CalledProcessError as err:
        print("Failed to generate")
        print(err)


print(f"Running in {args.jobs} threads")

blocks_iter = iter(blocks)

if args.progress:
    from tqdm.auto import tqdm
    blocks_iter = tqdm(blocks)

futures = []

with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
    for b in blocks_iter:
        futures.append(executor.submit(process_block, b))

        if len(futures) % args.jobs == 0:
            concurrent.futures.wait(futures)

            futures = []
