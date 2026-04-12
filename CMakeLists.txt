cmake_minimum_required(VERSION 3.20)
project(rc700-bios C)

# Detect if we are cross-compiling for Z80
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "z80")
    set(TARGET_Z80 TRUE)
else()
    set(TARGET_Z80 FALSE)
endif()

add_subdirectory(src)

if(NOT TARGET_Z80)
    enable_testing()
    add_subdirectory(test)
endif()
