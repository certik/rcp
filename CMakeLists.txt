project(rcp)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "-g -Wall -Wfatal-errors")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/src)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/tmp/usr/lib/)

add_subdirectory(src)
add_subdirectory(examples)
