cmake_minimum_required(VERSION 3.16)
project(poker_solver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(poker_solver
    src/main.cpp
    src/poker_engine.cpp
)

target_include_directories(poker_solver PRIVATE include)

add_executable(poker_api_server
    src/api_server.cpp
    src/poker_engine.cpp
)

target_include_directories(poker_api_server PRIVATE include)

if (MSVC)
    target_compile_options(poker_solver PRIVATE /W4)
    target_compile_options(poker_api_server PRIVATE /W4)
else()
    target_compile_options(poker_solver PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(poker_api_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
