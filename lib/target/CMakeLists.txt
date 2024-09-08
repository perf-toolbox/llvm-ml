file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/llvm-ml/target)

add_custom_target(target_includes)
add_custom_command(TARGET target_includes
  COMMAND ${CMAKE_COMMAND} -E copy 
    ${CMAKE_CURRENT_SOURCE_DIR}/Target.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/llvm-ml/target
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Target.hpp
)

add_llvm_library(target STATIC
        Target.cpp
        X86Target.cpp
        RISCVTarget.cpp
  LINK_COMPONENTS
  MC
  MCDisassembler
  Support
  Target
)

add_dependencies(target target_includes)

target_include_directories(target PRIVATE
  "${LLVM_DIR}/../../Target/X86/"
  "${LLVM_DIR}/../../Target/RISCV/"
  "${LLVM_DIR}/../../../../llvm/lib/Target/X86/"
  "${LLVM_DIR}/../../../../llvm/lib/Target/RISCV/"
)
target_include_directories(target PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
