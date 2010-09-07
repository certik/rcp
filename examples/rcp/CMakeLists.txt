include_directories(${rcp_SOURCE_DIR}/src)
add_executable(rcp main.cpp)
target_link_libraries(rcp teuchosmm)
