add_llvm_library(target STATIC
        "Target.cpp"
        "X86Target.cpp"
        RISCVTarget.cpp
)
target_link_libraries(target PUBLIC
    LLVMMC
    LLVMMCDisassembler
    LLVMMCParser
    LLVMSupport
    LLVMTarget
    LLVMX86CommonTableGen
    LLVMX86UtilsAndDesc
    LLVMRISCVCommonTableGen
    LLVMRISCVUtilsAndDesc
)
target_include_directories(target PRIVATE
  "${LLVM_DIR}/../../Target/X86/"
  "${LLVM_DIR}/../../Target/RISCV/"
  "${LLVM_DIR}/../../../../llvm/lib/Target/X86/"
  "${LLVM_DIR}/../../../../llvm/lib/Target/RISCV/"
)
