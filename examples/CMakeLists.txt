cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

project(mem_examples CXX)

add_executable(mem_example_0
	example_0.cpp)

target_link_libraries(mem_example_0
	mem)

set_target_properties(mem_example_0 PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON)
