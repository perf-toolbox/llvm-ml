cmake_minimum_required(VERSION 3.24)

project(llvm-ml C CXX)

set(CMAKE_CXX_STANDARD 20)

include(cmake/CPM.cmake)

CPMAddPackage("gh:p-ranav/indicators@2.3")
CPMAddPackage("gh:nlohmann/json@3.10.5")
CPMAddPackage("gh:catchorg/Catch2@3.4.0")
CPMAddPackage("gh:ericniebler/range-v3#53c40dd628450c977ee1558285ff43e0613fa7a9")
CPMAddPackage("gh:capnproto/capnproto#v2")

find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)

message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

include(TableGen)
include(AddLLVM)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Include directory: ${LLVM_INCLUDE_DIRS}")

add_subdirectory(lib/structures)
add_subdirectory(lib/target)
