# Leftover silicon included from repo-root project(machines).
# No project(). No cmake_minimum_required().
#
# MACHINES_LEFTOVER = apple2 | c64  (library target prefix)
# MACHINES_TEST_NS  = a2m | c64m    (test executable / ctest name prefix)
# Library targets: apple2_machine vs c64_machine (CMake names are global).
# Product binaries keep names a2m / c64m.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR
        "Nested leftover project() is retired. From the machines repo root:\n"
        "  cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug")
endif()

if(NOT MACHINES_LEFTOVER)
    message(FATAL_ERROR "MACHINES_LEFTOVER is not set (apple2 or c64)")
endif()
if(NOT MACHINES_TEST_NS)
    message(FATAL_ERROR "MACHINES_TEST_NS is not set (a2m or c64m)")
endif()

set(LEFTOVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT MACHINES_ROOT)
    set(MACHINES_ROOT "${CMAKE_SOURCE_DIR}")
endif()

set(machine  ${MACHINES_LEFTOVER}_machine)
set(util     ${MACHINES_LEFTOVER}_util)
set(runtime  ${MACHINES_LEFTOVER}_runtime)
set(platform ${MACHINES_LEFTOVER}_platform)
set(control  ${MACHINES_LEFTOVER}_control)
set(frontend ${MACHINES_LEFTOVER}_frontend)
set(tools    ${MACHINES_LEFTOVER}_tools)
set(d64      ${MACHINES_LEFTOVER}_d64)
set(g64      ${MACHINES_LEFTOVER}_g64)
set(t64      ${MACHINES_LEFTOVER}_t64)
set(crt      ${MACHINES_LEFTOVER}_crt)

# leftover_exe(test_audio_buffer sources...) -> a2m_test_audio_buffer
# Sets ${test_audio_buffer} in the caller for later target_* / add_test.
macro(leftover_exe name)
    add_executable(${MACHINES_TEST_NS}_${name} ${ARGN})
    set(${name} ${MACHINES_TEST_NS}_${name})
endmacro()

# leftover_add_test(NAME audio_buffer COMMAND ...) -> ctest name a2m.audio_buffer
function(leftover_add_test)
    set(_args ${ARGV})
    list(FIND _args NAME _idx)
    if(_idx EQUAL -1)
        message(FATAL_ERROR "leftover_add_test requires NAME")
    endif()
    math(EXPR _nidx "${_idx} + 1")
    list(GET _args ${_nidx} _name)
    list(REMOVE_AT _args ${_nidx})
    list(INSERT _args ${_nidx} "${MACHINES_TEST_NS}.${_name}")
    add_test(${_args})
endfunction()

function(leftover_label_tests)
    get_property(_tests DIRECTORY PROPERTY TESTS)
    foreach(_t ${_tests})
        set_property(TEST ${_t} APPEND PROPERTY LABELS ${MACHINES_TEST_NS})
    endforeach()
endfunction()
