cmake_minimum_required(VERSION 3.16)
project(cpp_unix_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minishell src/main.cpp)
target_compile_options(minishell PRIVATE -Wall -Wextra -Wpedantic)
