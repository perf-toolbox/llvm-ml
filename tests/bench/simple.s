; RUN: %mc-harness-dump --triple x86_64-unknown-unknown %s | FileCheck %s
; RUN: %mc-harness-dump --triple x86_64-unknown-unknown %s --num-repeat 120 --num-repeat-noise 42 | FileCheck %s --check-prefix=REP

imulq    $1374389535, %rax, %rax

; CHECK-LABEL: @baseline
; REP-LABEL: @baseline
; CHECK: call void asm alignstack "jmp workload_start_baseline", "~{dirflag},~{fpsr},~{flags}"()
; CHECK-NEXT: call void asm alignstack "workload_start_baseline:", ""() 
; CHECK-COUNT-10: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; REP-COUNT-42: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; CHECK: call void asm alignstack "workload_end_baseline:", ""() 

; CHECK-LABEL: @workload
; REP-LABEL: @workload
; CHECK: call void asm alignstack "jmp workload_start_workload", "~{dirflag},~{fpsr},~{flags}"()
; CHECK-NEXT: call void asm alignstack "workload_start_workload:", ""() 
; CHECK-COUNT-200: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; REP-COUNT-120: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; CHECK: call void asm alignstack "workload_end_workload:", ""() 
