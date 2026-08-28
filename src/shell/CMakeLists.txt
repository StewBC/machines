# Library only. No project(). No cmake_minimum_required().
# Inherits 3.16 policies from a2m or 3.28 from c64m.

add_library(shell STATIC
    util/mutex.c
    util/thread.c
    util/cond.c
    util/audio_buffer.c
    util/message_queue.c
    util/config.c
    util/host_log.c
    util/util.c
    platform/platform.c
    platform/platform_fs.c
    platform/platform_socket.c
    frontend/nuklear_impl.c
)

set_source_files_properties(util/audio_buffer.c PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_include_directories(shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    ${SDL2_INCLUDE_DIRS}
)

target_compile_features(shell PUBLIC c_std_99)

target_link_libraries(shell
    PUBLIC
        SDL2::SDL2
        logc
    PRIVATE
        inih
        stb_ds
)

if(WIN32)
    target_link_libraries(shell PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(shell PRIVATE /wd4116 /wd5287)
endif()

add_subdirectory(tools/am65)
add_subdirectory(tools/disasm_6502)
add_subdirectory(tools/symbols)
