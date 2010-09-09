include_directories(${rcp_SOURCE_DIR}/src)
add_executable(args main.cpp)
target_link_libraries(args teuchosmm)
