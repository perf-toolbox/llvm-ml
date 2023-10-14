import sqlite3
import argparse
import os
from llvm_ml.numpy import load_numpy_dataset
from tqdm import tqdm
import subprocess
import tempfile
import re

parser = argparse.ArgumentParser(
    prog="mc-llvm-mca-importer",
    description="Import llvm-mca estimations into sqlite format",
)

parser.add_argument("filename")
parser.add_argument("-o", "--output", required=True)

args = parser.parse_args()

assert os.path.exists(args.filename)

conn = sqlite3.connect(args.output)
cur = conn.cursor()

def get_llvm_mca_estimation(source, triple, cpu):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.s', delete=False) as tmp:
        tmp.write(source)
        file_name = tmp.name
        tmp.close()
        try:
            out = subprocess.check_output(["llvm-mca", "--mtriple", triple, "--mcpu", cpu, "--iterations", "100", file_name])
            m = re.search(r"Total Cycles:\s+([0-9]+)", str(out))
            os.remove(file_name)
            return float(m.group(1)) / 100.0
        except Exception as e:
            print(f"Exception! {e}")
            os.remove(file_name)
            return 0.0

    return 0.0

def process_single_source(id, source):
    est = get_llvm_mca_estimation(source, "x86_64-unknown-unknown", "znver2")
    cur.execute("""update samples SET llvm_mca_estimation = ?
                   where id = ?;
                """, (est, id))
    conn.commit()

dataset = load_numpy_dataset(args.filename)

for bb in tqdm(dataset):
    process_single_source(bb.id, bb.source)

conn.close()

