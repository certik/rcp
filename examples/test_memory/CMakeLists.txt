include_directories(${rcp_SOURCE_DIR}/src)
add_executable(test_memory main.cpp)
target_link_libraries(test_memory teuchosmm bfd-d iberty z)
