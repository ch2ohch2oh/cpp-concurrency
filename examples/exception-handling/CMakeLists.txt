cmake_minimum_required(VERSION 3.10)
project(exception-handling-examples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add examples
add_executable(01-exception-in-thread 01-exception-in-thread.cpp)
add_executable(02-exception-with-promise 02-exception-with-promise.cpp)
add_executable(03-exception-with-async 03-exception-with-async.cpp)
add_executable(04-thread-pool-exception-handling 04-thread-pool-exception-handling.cpp)
add_executable(05-exception-safe-task-runner 05-exception-safe-task-runner.cpp)

# Link pthread for threading support
target_link_libraries(01-exception-in-thread pthread)
target_link_libraries(02-exception-with-promise pthread)
target_link_libraries(03-exception-with-async pthread)
target_link_libraries(04-thread-pool-exception-handling pthread)
target_link_libraries(05-exception-safe-task-runner pthread)
