; RUN: %mc-harness-dump --triple x86_64-unknown-unknown %s | FileCheck %s

imulq    $1374389535, %rax, %rax

; CHECK-LABEL: @baseline
; CHECK: call void asm alignstack "jmp workload_start_baseline", "~{dirflag},~{fpsr},~{flags}"()
; CHECK-NEXT: call void asm alignstack "workload_start_baseline:", ""() 
; CHECK-COUNT-20: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; CHECK: call void asm alignstack "workload_end_baseline:", ""() 

; CHECK-LABEL: @workload
; CHECK: call void asm alignstack "jmp workload_start_workload", "~{dirflag},~{fpsr},~{flags}"()
; CHECK-NEXT: call void asm alignstack "workload_start_workload:", ""() 
; CHECK-COUNT-20: call void asm sideeffect "imulq    $$1374389535, %rax, %rax", "~{dirflag},~{fpsr},~{flags}"()
; CHECK: call void asm alignstack "workload_end_workload:", ""() 
