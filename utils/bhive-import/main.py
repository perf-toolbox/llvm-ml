import argparse
import csv
from pathlib import Path
import subprocess
import os.path
from tqdm import tqdm

parser = argparse.ArgumentParser(
    prog='bhive-import'
)

parser.add_argument('filename')
parser.add_argument('--prefix', type=str, required=True)
parser.add_argument('-o', '--output', type=str, required=True)

args = parser.parse_args()

if not Path(args.filename).is_file():
    print(f"{args.filename} does not exist")
    exit(1)

if not Path(args.output).is_dir():
    print(f"{args.output} does not exist")
    exit(1)

with open(args.filename, 'r') as csvfile:
    reader = csv.reader(csvfile, delimiter=',')
    num = 0
    for row in tqdm(reader):
        bytes = []
        hex = row[0]
        for i in range(0, len(hex), 2):
            byte = hex[i:i+2]
            bytes.append('0x'+byte)

        cmd = f"echo {' '.join(bytes)} | llvm-mc -disassemble --triple=x86_64-unknown-unknown"
        stdout = subprocess.check_output(cmd, shell=True)
        asm = stdout.decode('utf8')
        first_line = asm.find('\n') + 1
        asm = asm[first_line:]
        result_file_path = os.path.join(args.output, args.prefix + f"_{num}")
        result_file = open(result_file_path, 'w')
        result_file.write(asm)
        result_file.close()
        num += 1