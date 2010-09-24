include_directories(${rcp_SOURCE_DIR}/src)
add_executable(show main.cpp)
target_link_libraries(show teuchosmm bfd-d iberty z)
