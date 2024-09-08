file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/llvm-ml/structures)

set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/llvm-ml/structures)
capnp_generate_cpp(structSources structHeaders
  mc_dataset.capnp
  mc_graph.capnp
  mc_metrics.capnp
)

add_custom_target(structures_includes)
add_custom_command(TARGET structures_includes
  COMMAND ${CMAKE_COMMAND} -E copy 
    ${CMAKE_CURRENT_SOURCE_DIR}/structures.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/llvm-ml/structures
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/structures.hpp
)

add_library(cpp_structures STATIC
  structures.cpp
  ${structSources}
  ${structHeaders}
)
target_link_libraries(cpp_structures PUBLIC CapnProto::capnp)
target_include_directories(cpp_structures PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
add_dependencies(cpp_structures structures_includes)
