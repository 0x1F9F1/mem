cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

project(mem_tests CXX)

file(GLOB MEM_HEADERS ../include/mem/*.h)

add_executable(${PROJECT_NAME}
    main.cpp
    tests.cpp

    doctest.h

    ${MEM_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    mem)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wswitch -Wswitch-enum -Woverloaded-virtual -Wundef -Wconversion-null -Wold-style-cast")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

add_test(mem_tests mem_tests)
