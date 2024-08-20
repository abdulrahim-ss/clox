cmake_minimum_required(VERSION 3.29)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(.
        src
        headers
)

add_executable(clox
        main.c
        src/chunk.c
        src/memory.c
        src/debug.c
        src/value.c
        src/vm.c
        src/scanner.c
        src/compiler.c
        headers/chunk.h
        headers/memory.h
        headers/debug.h
        headers/value.h
        headers/vm.h
        headers/compiler.h
        headers/object.h
)
