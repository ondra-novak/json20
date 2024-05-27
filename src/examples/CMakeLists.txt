cmake_minimum_required(VERSION 3.5)

add_executable(example json20.cpp)
add_dependencies(example libjson20_single_header)

