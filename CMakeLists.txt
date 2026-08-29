cmake_minimum_required(VERSION 3.24)

# Two-target monorepo build. Nested project(a2m) / project(c64m) is not the
# CI entry. 3.24 covers CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY
# (c64m's former 3.28 line was a version gate only; Stage 0 configured on 3.24).
project(machines LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Product binaries are ./build/a2m, ./build/c64m, ./build/am65. A leftover
# nested cmake tree at those paths makes ld fail with "Is a directory".
foreach(_exe a2m c64m am65)
    if(IS_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_exe}")
        message(FATAL_ERROR
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_exe} is a directory. "
            "Remove leftover nested build trees (`rm -rf build/a2m build/c64m`); "
            "do not configure -S src/apple2 or -S src/c64.")
    endif()
endforeach()

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
    add_compile_definitions(SDL_MAIN_HANDLED)
    add_link_options(/STACK:8388608)
endif()

option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN are mutually exclusive")
endif()

if(ENABLE_ASAN)
    message(STATUS "Building with AddressSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN)
    message(STATUS "Building with ThreadSanitizer")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

enable_testing()

find_package(SDL2 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(MACHINES_ROOT "${CMAKE_SOURCE_DIR}")

add_subdirectory(external)
add_subdirectory(src/shell)

# Leftover silicon: library + product executable + tests. No project().
set(MACHINES_LEFTOVER apple2)
set(MACHINES_TEST_NS a2m)
add_subdirectory(src/apple2 apple2)

set(MACHINES_LEFTOVER c64)
set(MACHINES_TEST_NS c64m)
add_subdirectory(src/c64 c64)
