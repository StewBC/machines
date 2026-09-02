# Library only. No project(). No cmake_minimum_required().
# Inherits 3.24 policies from repo-root project(machines).

# Tool targets before libshell so panes can PUBLIC-link disasm_6502.
add_subdirectory(tools/am65)
add_subdirectory(tools/disasm_6502)
add_subdirectory(tools/symbols)

add_library(shell STATIC
    util/mutex.c
    util/thread.c
    util/cond.c
    util/audio_buffer.c
    util/message_queue.c
    util/config.c
    util/host_log.c
    util/host_page_writer.c
    util/host_page_name.c
    platform/platform.c
    platform/platform_fs.c
    platform/platform_socket.c
    frontend/nuklear_impl.c
    frontend/disk_led_data.c
    frontend/forensics_view.c
    frontend/symbol_lookup_view.c
    frontend/debugger_layout.c
    frontend/debugger_context_menu.c
    frontend/disasm_pc_lock.c
    frontend/debugger_disasm.c
    frontend/memory_search.c
    frontend/cpu_pane_6502.c
    frontend/disasm_pane.c
    frontend/memview_pane.c
    frontend/breakpoint_chrome.c
    frontend/window_title.c
    frontend/inspector_tab.c
    control/control_framing.c
    control/control_command_table.c
    control/memory_source.c
    runtime/runtime_history.c
    runtime/runtime_history_query_parse.c
    runtime/runtime_history_wire.c
    runtime/runtime_breakpoint_condition.c
    runtime/runtime_symbol_snapshot.c
)

set_source_files_properties(util/audio_buffer.c PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_include_directories(shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/control
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${SDL2_INCLUDE_DIRS}
)

target_compile_features(shell PUBLIC c_std_99)

target_link_libraries(shell
    PUBLIC
        SDL2::SDL2
        logc
        disasm_6502
        symbols
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
