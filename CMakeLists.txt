cmake_minimum_required(VERSION 3.10)
project(compiler)

# Ensure C++17 for the project
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable and its source files located in the src/ directory
add_executable(compiler
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/semantic.cpp
    src/ir.cpp
    src/optimizer.cpp
    src/cfg.cpp
)

# Tell CMake where to find the .hpp header files
target_include_directories(compiler PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(CTest)

if(BUILD_TESTING)
    add_test(
        NAME compiler_example
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/example1.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_example"
    )

    add_test(
        NAME compiler_comment_support
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/comment_support.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_comment_support"
    )

    add_test(
        NAME compiler_missing_main
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/missing_main.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_missing_main"
    )
    set_tests_properties(compiler_missing_main PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME compiler_function_calls
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/function_calls.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_function_calls"
    )

    add_test(
        NAME compiler_bad_arity
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/bad_arity.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_bad_arity"
    )
    set_tests_properties(compiler_bad_arity PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME compiler_undefined_function
        COMMAND $<TARGET_FILE:compiler>
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/undefined_function.src"
                "${CMAKE_CURRENT_BINARY_DIR}/compiler_undefined_function"
    )
    set_tests_properties(compiler_undefined_function PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME compiler_optimizations
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_optimizations.ps1"
                "$<TARGET_FILE:compiler>"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/optimizations.src"
                "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME compiler_diagnostics
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_diagnostics.ps1"
                "$<TARGET_FILE:compiler>"
                "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_test(
        NAME compiler_golden_outputs
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_golden_outputs.ps1"
                "$<TARGET_FILE:compiler>"
                "${CMAKE_CURRENT_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}"
    )

    add_test(
        NAME compiler_random_parser_smoke
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/random_parser_smoke.ps1"
                "$<TARGET_FILE:compiler>"
    )

    add_test(
        NAME compiler_loop_transforms
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_loop_transforms.ps1"
                "$<TARGET_FILE:compiler>"
                "${CMAKE_CURRENT_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()
