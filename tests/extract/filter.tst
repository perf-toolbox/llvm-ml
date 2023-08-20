# UNSUPPORTED: system-windows
# RUN: mkdir -p %t.dir
# RUN: cp %S/Inputs/filter/*.s %t.dir/
# RUN: %llvm-mc-extract --prefix test --asm-dir %t.dir/ --postprocess-only --triple=x86_64-unknown-unknown
# RUN: ls -lah %t.dir/ | FileCheck %s

# CHECK-NOT: cpuid.s
# CHECK-NOT: rep.s
# CHECK-NOT: single_instr.s
# CHECK-DAG: add.s
