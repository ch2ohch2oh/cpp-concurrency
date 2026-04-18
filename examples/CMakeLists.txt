cmake_minimum_required(VERSION 3.10)
project(examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically add all example subdirectories that have a CMakeLists.txt
file(GLOB CMAKE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt")
foreach(CMAKE_FILE ${CMAKE_FILES})
    get_filename_component(EXAMPLE_DIR ${CMAKE_FILE} DIRECTORY)
    add_subdirectory(${EXAMPLE_DIR})
endforeach()
