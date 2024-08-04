cmake_minimum_required(VERSION 3.29)
project(clox C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(clox
        main.c
        chunk.c
        memory.c
        debug.c
        value.c
        vm.c
        chunk.h
        memory.h
        debug.h
        value.h
        vm.h
)
