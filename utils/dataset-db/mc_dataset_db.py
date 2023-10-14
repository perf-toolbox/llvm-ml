import sqlite3
import argparse
import os
from llvm_ml.numpy import load_numpy_dataset
from tqdm import tqdm

parser = argparse.ArgumentParser(
    prog="mc-dataset-db",
    description="Import dataset into sqlite format",
)

parser.add_argument("filename")
parser.add_argument("-o", "--output", required=True)

args = parser.parse_args()

assert os.path.exists(args.filename)

needs_setup = not os.path.exists(args.output)

conn = sqlite3.connect(args.output)
cur = conn.cursor()

if needs_setup:
    cur.execute("""create table samples(
        id                  TEXT not null
            constraint table_name_pk
                primary key,
        origin              TEXT not null,
        asm                 TEXT,
        cov                 REAL not null,
        measured_cycles     REAL not null,
        has_virtual_root    integer not null,
        llvm_mca_estimation REAL
    );""")

dataset = load_numpy_dataset(args.filename)

for bb in tqdm(dataset):
    origin = bb.id.split("_")[0]
    hvr = 1 if bb.has_virtual_root else 0
    cur.execute("""insert or ignore into samples
                    (id, origin, asm, cov, measured_cycles, has_virtual_root)
                    values (?, ?, ?, ?, ?, ?);
                """, (bb.id, origin, bb.source, bb.cov, bb.measured_cycles, hvr))
    conn.commit()
conn.close()
