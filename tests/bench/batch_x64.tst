# UNSUPPORTED: system-windows
# FIXME(Alex) figure out why test takes forever inside LIT
# REQUIRES: x86_64, skipped
# RUN: mkdir -p %t.out
# RUN: env LLVM_ML_BENCH_MOCK=1 %llvm-mc-bench -c 0 %S/Inputs/x64 -o %t.out
# RUNx: ls -1 | wc -l | FileCheck %s

# CHECK: 2
