#cmake_minimum_required(VERSION 4.1)
cmake_minimum_required(VERSION 3.26)

#project(flux)
project(flux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Ensure all executables (including tests) go to build/Debug or build/Release as appropriate
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel)

add_executable(flux
        src/main.cpp
        src/lexer/lexer.h
        src/lexer/lexer.cpp
        src/lexer/token.h
        src/lexer/diagnostic.h
        src/ast/ast.h
        src/parser/parser.h
        src/parser/parser.cpp
        src/ast/ast_printer.h
        src/ast/ast_printer.cpp
        src/semantic/symbol.h
        src/semantic/scope.h
        src/semantic/resolver.h
        src/semantic/resolver.cpp
        src/semantic/type.h
)

target_include_directories(flux PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(flux PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Simple test executable for numeric promotion helpers
add_executable(numeric_tests
        tests/numeric_promotion.cpp
        src/semantic/resolver.cpp
        src/semantic/resolver.h
        src/semantic/type.h
)

target_include_directories(numeric_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(numeric_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Tuple test executable
add_executable(tuple_test
        tests/tuple.cpp
        src/semantic/resolver.cpp
        src/semantic/resolver.h
        src/semantic/type.h
        src/ast/ast.h
)

target_include_directories(tuple_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(tuple_test PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

enable_testing()
add_test(NAME numeric_tests COMMAND numeric_tests)
add_test(NAME tuple_test COMMAND tuple_test)

# Struct field test executable
add_executable(struct_field_test
        tests/struct_field.cpp
        src/semantic/resolver.cpp
        src/semantic/resolver.h
        src/semantic/type.h
)

target_include_directories(struct_field_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(struct_field_test PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_test(NAME struct_field_test COMMAND struct_field_test)

# Array/slice type test executable
add_executable(array_slice_test
    tests/array_slice.cpp
    src/semantic/resolver.cpp
    src/semantic/resolver.h
    src/semantic/type.h
    src/ast/ast.h
)

target_include_directories(array_slice_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(array_slice_test PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
target_compile_features(array_slice_test PRIVATE cxx_std_20)

add_test(NAME array_slice_test COMMAND array_slice_test)
