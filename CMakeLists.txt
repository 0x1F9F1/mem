cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

option(MEM_TEST "Generate the test target." OFF)

project(mem CXX)

add_library(mem INTERFACE)

target_include_directories(mem INTERFACE
    include)

if (MEM_TEST)
    enable_testing()

    add_subdirectory(tests)
    add_subdirectory(examples)
endif ()
