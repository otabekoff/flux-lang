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
    src/semantic/monomorphizer.cpp
    src/ir/ir_builder.cpp
    src/ir/ir_lowering.cpp
    src/ir/ir_printer.cpp
    src/ir/passes/constant_folding.cpp
    src/ir/passes/dead_code_elimination.cpp
    src/ir/passes/ir_verifier.cpp
    src/ir/passes/inliner.cpp
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
# LLVM Integration
# --------------------------------------------------
set(LLVM_INSTALL_DIR "C:/Program Files/LLVM")
set(LLVM_SRC_DIR "C:/libs/llvm-project-18.1.8/llvm")

if(EXISTS "${LLVM_SRC_DIR}/include/llvm-c/Core.h")
    message(STATUS "Using LLVM headers from: ${LLVM_SRC_DIR}/include")
    include_directories("${LLVM_SRC_DIR}/include")
    link_directories("${LLVM_INSTALL_DIR}/lib")
    set(LLVM_LIBS "LLVM-C")
else()
    message(STATUS "LLVM source headers not found, searching system...")
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    
    llvm_map_components_to_libnames(llvm_libs core support native)
    set(LLVM_LIBS ${llvm_libs})
    
    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})
endif()


# --------------------------------------------------
# Codegen library
# --------------------------------------------------
add_library(flux_codegen
    src/codegen/codegen.cpp
    src/codegen/type_converter.cpp
)

target_include_directories(flux_codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(flux_codegen PUBLIC 
    flux_core 
    ${LLVM_LIBS}
)


# --------------------------------------------------
# Helper function for tests
# --------------------------------------------------
function(add_flux_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE flux_core)
    # If test needs codegen, we can link flux_codegen too. 
    # For now default is flux_core. 
    # We will see if we need separate helper for codegen tests.
endfunction()

function(add_codegen_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE flux_codegen flux_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

enable_testing()

add_flux_test(numeric_promotion)
add_flux_test(tuple)
# ... existing tests ...
# I will keep existing add_flux_test calls unchanged and append others.
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
add_flux_test(move_semantics)
add_flux_test(trait_conformance)
add_flux_test(trait_default_methods)
add_flux_test(trait_dispatch)
add_flux_test(index_slice)
add_flux_test(generic_method)
add_flux_test(generic_method_fail)
add_flux_test(trait_default_generic)
add_flux_test(visibility)
add_flux_test(orphan_rule)
add_flux_test(pattern_matching)
add_flux_test(control_flow)
add_flux_test(error_handling)
add_flux_test(ownership_borrowing)
add_flux_test(concurrency)
add_flux_test(hardening_phase1)
add_flux_test(ir_basic)

add_codegen_test(codegen_basic)


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

