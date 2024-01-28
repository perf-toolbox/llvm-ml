from llvm_ml.torch import BasicBlockDataset
import csv
import matplotlib.pyplot as plt
import numpy as np
import sys

with open(sys.argv[1]) as f:
    reader = csv.reader(f)
    vocab = [row[1] for row in reader]

banned_ids = []
dataset = BasicBlockDataset(sys.argv[2], vocab, masked=False, banned_ids=banned_ids, prefilter=True)

hist = [0 for _ in range(dataset.num_opcodes)]

for i in range(dataset.len()):
    bb, _, _, _ = dataset.get(i)

    for node in bb.x:
        hist[node] += 1

hist = np.array(hist, dtype=float)
hist_sum = np.sum(hist)
hist = hist / hist_sum
hist.tofile(sys.argv[3], sep="\n")