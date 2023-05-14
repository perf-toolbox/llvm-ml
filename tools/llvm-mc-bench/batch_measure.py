import argparse
import subprocess
import os
import sys
from tqdm import tqdm

parser = argparse.ArgumentParser(
            prog="Basic block batch benchmark"
        )
parser.add_argument("input")
parser.add_argument("output")
parser.add_argument("--cpu", default=1, type=int, required=True)
parser.add_argument("-n", "--num_runs", default=200, type=int)
parser.add_argument("--start", default=0, type=int)
parser.add_argument("--end", default=0, type=int)

args = parser.parse_args()

if sys.platform == 'linux':
    with open('/proc/sys/kernel/perf_event_paranoid', 'r') as f:
        val = f.read()
        if val.strip() != '-1':
            print("Enable system profiling: echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid")
            exit()

if not os.path.exists(args.output) and not os.path.isdir(args.output):
    print(f"Output directory does not exist: {args.output}")
    exit(1)

inputs = sorted(os.listdir(args.input))

if args.end != 0:
    inputs = inputs[args.start:args.end]
else:
    inputs = inputs[args.start:]

for bbf in tqdm(inputs):
    out_name = os.path.basename(bbf)
    out_path = os.path.join(args.output, out_name + ".pb")
    input_path = os.path.join(args.input, bbf) 
    try:
        subprocess.run(["./bazel-bin/tools/llvm-mc-bench", input_path, "-o", out_path, "-c", str(args.cpu), "-n", str(args.num_runs)], timeout=10, check=True, capture_output=True)
    except subprocess.CalledProcessError as e:
        print(f"./bazel-bin/tools/llvm-mc-bench {input_path} -o {out_path} -c {args.cpu} -n {args.num_runs}")
        print(e.stderr)
        print(e.stdout)
    except subprocess.TimeoutExpired:
        pass

