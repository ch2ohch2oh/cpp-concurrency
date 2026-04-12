cmake_minimum_required(VERSION 3.10)
project(examples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add individual example directories as they are created
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/22-exception-handling")
    add_subdirectory(22-exception-handling)
endif()
