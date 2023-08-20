# UNSUPPORTED: system-windows
# REQUIRES: x86_64
# RUN: mkdir -p %t.out
# RUN: env LLVM_ML_BENCH_MOCK=1 %llvm-mc-bench -c 0 %S/Inputs/x64 --num-repeat 20 -o %t.out
# RUN: ls -1 %t.out | wc -l | FileCheck %s

# CHECK: 2
