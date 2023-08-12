# UNSUPPORTED: system-windows
# FIXME(Alex) figure out why test takes forever inside LIT
# REQUIRES: x86_64
# RUN: env LLVM_ML_BENCH_MOCK=1 %llvm-mc-bench -c 0 %S/Inputs/x64/01.s --num-repeat 20 -o %t.cbuf
# RUN: ls %t.cbuf
# RUN: env LLVM_ML_BENCH_MOCK=1 %llvm-mc-bench -c 0 %S/Inputs/x64/01.s --num-repeat 20 -o %t.json --readable-json
# RUN: ls %t.json

