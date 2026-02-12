cmake_minimum_required(VERSION 3.26)
# cmake_minimum_required(VERSION 4.21)

#project(flux)
project(flux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
# Ensure all executables (including tests) go to build/Debug or build/Release as appropriate
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel)


# --------------------------------------------------
# Common compiler settings as an interface target
# --------------------------------------------------
add_library(flux_warnings INTERFACE)

target_compile_options(flux_warnings INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# --------------------------------------------------
# Core library (shared by all executables)
# --------------------------------------------------
add_library(flux_core
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/ast/ast_printer.cpp
    src/semantic/resolver.cpp
)

target_include_directories(flux_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(flux_core PUBLIC flux_warnings)

# --------------------------------------------------
# Main executable
# --------------------------------------------------
add_executable(flux src/main.cpp)
target_link_libraries(flux PRIVATE flux_core)


# --------------------------------------------------
# Helper function for tests
# --------------------------------------------------
function(add_flux_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE flux_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

enable_testing()

add_flux_test(numeric_promotion)
add_flux_test(tuple)
add_flux_test(struct_field)
add_flux_test(array_slice)

add_flux_test(reference)
add_flux_test(lambda_parse)

# Register Option/Result type test
add_flux_test(option_result)
add_flux_test(type_alias)
add_flux_test(never_propagation)
add_flux_test(trait_bound)
add_flux_test(where_clause)
add_flux_test(monomorphization)
add_flux_test(associated_types)
add_flux_test(trait_conformance)
add_flux_test(trait_default_methods)
add_flux_test(trait_dispatch)
add_flux_test(index_slice)


# By this what we've earned?
#
# flux_core   ← compiler library
# flux        ← CLI frontend
# tests/*     ← test executables
#
# This makes it easy to later add:
#
# flux_lsp
# flux_formatter
# flux_repl
# flux_jit
#
# All linking to the same flux_core.
#
# When the project grows, move to this:
#
# CMakeLists.txt
# src/
#     CMakeLists.txt
#     lexer/
#     parser/
#     ast/
#     semantic/
# tests/
#     CMakeLists.txt
#
# Then the root file becomes very small:
#
# add_subdirectory(src)
# add_subdirectory(tests)

