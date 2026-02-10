#cmake_minimum_required(VERSION 4.1)
cmake_minimum_required(VERSION 3.26)

#project(flux)
project(flux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#add_executable(flux main.cpp)

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