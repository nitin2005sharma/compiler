cmake_minimum_required(VERSION 3.10)
project(compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(compiler
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/semantic.cpp
    src/ir.cpp
    src/cfg.cpp
)

target_include_directories(compiler PRIVATE ${CMAKE_SOURCE_DIR}/include)