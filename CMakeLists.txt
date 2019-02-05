cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

option(MEM_TEST "Generate the test target." OFF)

project(mem CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    include)

if (MEM_TEST)
    enable_testing()

    add_subdirectory(tests)
endif ()
