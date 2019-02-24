cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(mem_examples CXX)

add_executable(mem_example_0 example_0.cpp)
target_link_libraries(mem_example_0 mem)
