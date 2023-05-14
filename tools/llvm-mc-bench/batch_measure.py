import argparse
import subprocess
import os
import sys

def create_task(basic_block, out_dir, cpu_num, retry, num_runs):
    out_name = os.path.basename(basic_block)
    out_path = os.path.join(out_dir, out_name + ".pb")
    handle = subprocess.Popen(["./bazel-bin/tools/llvm-mc-bench", basic_block, "-o", out_path, "-c", str(cpu_num), "-n", str(num_runs)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    task = {}
    task["proc"] = handle
    task["basic_block"] = basic_block
    task["cpu"] = cpu_num
    task["retries"] = retry

    return task

parser = argparse.ArgumentParser(
            prog="Basic block batch benchmark"
        )
parser.add_argument("input")
parser.add_argument("output")
parser.add_argument("-cpus", default=1, type=str, required=True)
parser.add_argument("-r", "--retries", default=5, type=int)
parser.add_argument("-n", "--num_runs", default=10000, type=int)
parser.add_argument("--start", default=0, type=int)
parser.add_argument("--end", default=0, type=int)
parser.add_argument("--progress", action="store_true")

args = parser.parse_args()

if sys.platform == 'linux':
    with open('/proc/sys/kernel/perf_event_paranoid', 'r') as f:
        val = f.read()
        if val.strip() != '-1':
            print("Enable system profiling: echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid")
            exit()

inputs = sorted(os.listdir(args.input))

if args.end != 0:
    inputs = inputs[args.start:args.end]
else:
    inputs = inputs[args.start:]

total = len(inputs)

tasks = []
cpus = args.cpus.split(",")

if args.progress:
    from tqdm import tqdm
    pbar = tqdm(total=total)

progress = 0
for bbf in inputs:
    while len(tasks) == len(args.cpus.split(",")):
        new_tasks = []
        cpus = args.cpus.split(",")

        for t in tasks:
            try:
                t["proc"].wait(10)
            except subprocess.TimeoutExpired:
                t["proc"].kill()
                continue
            if t["proc"].returncode != 0 and t["retries"] + 1 < args.retries:
                new_tasks.append(create_task(t["basic_block"], args.output, t["cpu"], t["retries"] + 1, args.num_runs))
                cpus.remove(t["cpu"])

        tasks = new_tasks
                
    cpu = cpus.pop()            
    tasks.append(create_task(os.path.join(args.input, bbf), args.output, cpu, 0, args.num_runs))
    progress += 1
    if args.progress:
        pbar.update()

