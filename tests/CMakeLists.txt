cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(mem_tests CXX)

add_executable(${PROJECT_NAME}
    main.cpp)

target_link_libraries(${PROJECT_NAME}
    mem gtest_main)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

add_test(mem_tests mem_tests)
