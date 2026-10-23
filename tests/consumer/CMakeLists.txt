cmake_minimum_required(VERSION 3.15)

# C is enough for the shared library, but the static library contains C++ code, so a project that links
# lzham::lzham_static must enable the CXX language for CMake to add the C++ runtime to the link line.
project(lzham_consumer C CXX)

find_package(lzham 1.0 REQUIRED)

add_executable(lzham_consumer main.c)
target_link_libraries(lzham_consumer PRIVATE lzham::lzham)

add_executable(lzham_consumer_static main.c)
target_link_libraries(lzham_consumer_static PRIVATE lzham::lzham_static)
