capnp_generate_cpp(structSources structHeaders
  mc_dataset.capnp
  mc_graph.capnp
  mc_metrics.capnp
)

add_library(cpp_structures STATIC
  structures.cpp
  ${structSources}
)
target_link_libraries(cpp_structures PUBLIC CapnProto::capnp)
target_include_directories(cpp_structures PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
