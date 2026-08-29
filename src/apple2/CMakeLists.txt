# Leftover Apple silicon + a2m executable + a2m tests.
# No project(). No cmake_minimum_required(). Included from repo-root project(machines).

include("${CMAKE_SOURCE_DIR}/cmake/leftover.cmake")

add_subdirectory(util)
add_subdirectory(machine)
add_subdirectory(runtime)
add_subdirectory(platform)
add_subdirectory(control)
add_subdirectory(tools)
add_subdirectory(frontend)

add_executable(a2m
    app_options.c
    main.c
)

target_compile_features(a2m PRIVATE c_std_99)
target_compile_definitions(a2m PRIVATE
    SDL_MAIN_HANDLED
    A2M_VERSION="3.1.0"
    A2M_SOURCE_DIR="${LEFTOVER_ROOT}"
)

target_include_directories(a2m PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Direct deps only: frontend PUBLIC-links runtime→machine→util and PRIVATE-links
# platform/tools/SDL. Re-listing those here duplicates on Apple ld.
target_link_libraries(a2m PRIVATE
    argparse
    ${frontend}
    ${control}
)

# --- util ------------------------------------------------------------------

leftover_exe(test_audio_buffer
    ${MACHINES_ROOT}/tests/shell/util/test_audio_buffer.c
)
target_compile_features(${test_audio_buffer} PRIVATE c_std_99)
target_link_libraries(${test_audio_buffer} PRIVATE ${util} SDL2::SDL2)
leftover_add_test(NAME audio_buffer COMMAND ${test_audio_buffer})

leftover_exe(test_message_queue
    ${MACHINES_ROOT}/tests/shell/util/test_message_queue.c
)
target_compile_features(${test_message_queue} PRIVATE c_std_99)
target_link_libraries(${test_message_queue} PRIVATE ${util} SDL2::SDL2)
leftover_add_test(NAME message_queue COMMAND ${test_message_queue})

leftover_exe(test_config
    ${MACHINES_ROOT}/tests/shell/util/test_config.c
)
target_compile_features(${test_config} PRIVATE c_std_99)
target_link_libraries(${test_config} PRIVATE ${util} SDL2::SDL2)
leftover_add_test(NAME config COMMAND ${test_config})

leftover_exe(test_fs_watch
    ${MACHINES_ROOT}/tests/apple2/util/test_fs_watch.c
)
target_compile_features(${test_fs_watch} PRIVATE c_std_99)
target_compile_definitions(${test_fs_watch} PRIVATE FS_WATCH_TESTING)
target_link_libraries(${test_fs_watch} PRIVATE ${util} SDL2::SDL2)
leftover_add_test(NAME fs_watch COMMAND ${test_fs_watch})

leftover_exe(test_memory_search
    ${MACHINES_ROOT}/tests/shell/frontend/test_memory_search.c
    ${MACHINES_ROOT}/src/shell/frontend/memory_search.c
)
target_include_directories(${test_memory_search} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)
target_compile_features(${test_memory_search} PRIVATE c_std_99)
leftover_add_test(NAME memory_search COMMAND ${test_memory_search})

leftover_exe(test_disasm_pc_lock
    ${MACHINES_ROOT}/tests/shell/frontend/test_disasm_pc_lock.c
    ${MACHINES_ROOT}/src/shell/frontend/disasm_pc_lock.c
)
target_include_directories(${test_disasm_pc_lock} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)
target_compile_features(${test_disasm_pc_lock} PRIVATE c_std_99)
target_link_libraries(${test_disasm_pc_lock} PRIVATE ${tools})
leftover_add_test(NAME disasm_pc_lock COMMAND ${test_disasm_pc_lock})

leftover_exe(test_window_title_shell
    ${MACHINES_ROOT}/tests/shell/frontend/test_window_title.c
    ${MACHINES_ROOT}/src/shell/frontend/window_title.c
)
target_include_directories(${test_window_title_shell} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)
target_compile_features(${test_window_title_shell} PRIVATE c_std_99)
leftover_add_test(NAME window_title_shell COMMAND ${test_window_title_shell})

leftover_exe(test_memory_source_cycle
    ${MACHINES_ROOT}/tests/shell/frontend/test_memory_source_cycle.c
)
target_compile_features(${test_memory_source_cycle} PRIVATE c_std_99)
target_link_libraries(${test_memory_source_cycle} PRIVATE shell)
leftover_add_test(NAME memory_source_cycle COMMAND ${test_memory_source_cycle})

leftover_exe(test_cpu_pane_6502
    ${MACHINES_ROOT}/tests/shell/frontend/test_cpu_pane_6502.c
)
target_compile_features(${test_cpu_pane_6502} PRIVATE c_std_99)
target_compile_definitions(${test_cpu_pane_6502} PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(${test_cpu_pane_6502} PRIVATE shell)
leftover_add_test(NAME cpu_pane_6502 COMMAND ${test_cpu_pane_6502})

leftover_exe(test_inspector_tab
    ${MACHINES_ROOT}/tests/shell/frontend/test_inspector_tab.c
)
target_compile_features(${test_inspector_tab} PRIVATE c_std_99)
target_compile_definitions(${test_inspector_tab} PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(${test_inspector_tab} PRIVATE shell)
leftover_add_test(NAME inspector_tab COMMAND ${test_inspector_tab})

leftover_exe(test_help_view
    ${MACHINES_ROOT}/tests/shell/frontend/test_help_view.c
)
target_include_directories(${test_help_view} PRIVATE
    ${LEFTOVER_ROOT}/frontend
    ${LEFTOVER_ROOT}/machine
)
target_compile_features(${test_help_view} PRIVATE c_std_99)
target_compile_definitions(${test_help_view} PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(${test_help_view} PRIVATE ${frontend})
leftover_add_test(NAME help_view COMMAND ${test_help_view})

leftover_exe(test_forensics_view
    ${MACHINES_ROOT}/tests/shell/frontend/test_forensics_view.c
)
target_compile_features(${test_forensics_view} PRIVATE c_std_99)
target_compile_definitions(${test_forensics_view} PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(${test_forensics_view} PRIVATE shell)
leftover_add_test(NAME forensics_view COMMAND ${test_forensics_view})

leftover_exe(test_frontend_input
    ${MACHINES_ROOT}/tests/apple2/frontend/test_frontend_input.c
    frontend/frontend_input.c
)
target_include_directories(${test_frontend_input} PRIVATE
    ${LEFTOVER_ROOT}/frontend
    ${LEFTOVER_ROOT}/machine
)
target_compile_features(${test_frontend_input} PRIVATE c_std_99)
target_link_libraries(${test_frontend_input} PRIVATE ${machine} SDL2::SDL2)
leftover_add_test(NAME frontend_input COMMAND ${test_frontend_input})

leftover_exe(test_crt_pixel_map
    ${MACHINES_ROOT}/tests/apple2/frontend/test_crt_pixel_map.c
    frontend/crt_renderer.c
)
target_include_directories(${test_crt_pixel_map} PRIVATE
    ${LEFTOVER_ROOT}/frontend
)
target_compile_features(${test_crt_pixel_map} PRIVATE c_std_99)
if(UNIX AND NOT APPLE)
    target_link_libraries(${test_crt_pixel_map} PRIVATE m)
endif()
leftover_add_test(NAME crt_pixel_map COMMAND ${test_crt_pixel_map})

leftover_exe(test_apple_type_script
    ${MACHINES_ROOT}/tests/apple2/util/test_apple_type_script.c
)
target_compile_features(${test_apple_type_script} PRIVATE c_std_99)
target_link_libraries(${test_apple_type_script} PRIVATE ${util})
leftover_add_test(NAME apple_type_script COMMAND ${test_apple_type_script})

leftover_exe(test_apple2_file
    ${MACHINES_ROOT}/tests/apple2/util/test_apple2_file.c
)
target_compile_features(${test_apple2_file} PRIVATE c_std_99)
target_link_libraries(${test_apple2_file} PRIVATE ${util})
leftover_add_test(NAME apple2_file COMMAND ${test_apple2_file})

# --- machine ---------------------------------------------------------------

leftover_exe(test_apple2_stub
    ${MACHINES_ROOT}/tests/apple2/machine/test_apple2_stub.c
)
target_compile_features(${test_apple2_stub} PRIVATE c_std_99)
target_link_libraries(${test_apple2_stub} PRIVATE ${machine})
leftover_add_test(NAME apple2_stub COMMAND ${test_apple2_stub})

leftover_exe(test_cpu65_basic
    ${MACHINES_ROOT}/tests/apple2/machine/test_cpu65_basic.c
)
target_compile_features(${test_cpu65_basic} PRIVATE c_std_99)
target_link_libraries(${test_cpu65_basic} PRIVATE ${machine})
leftover_add_test(NAME cpu65_basic COMMAND ${test_cpu65_basic})

leftover_exe(test_softswitch
    ${MACHINES_ROOT}/tests/apple2/machine/test_softswitch.c
)
target_compile_features(${test_softswitch} PRIVATE c_std_99)
target_link_libraries(${test_softswitch} PRIVATE ${machine})
leftover_add_test(NAME softswitch COMMAND ${test_softswitch})

leftover_exe(test_rom_boot
    ${MACHINES_ROOT}/tests/apple2/machine/test_rom_boot.c
)
target_compile_features(${test_rom_boot} PRIVATE c_std_99)
target_link_libraries(${test_rom_boot} PRIVATE ${machine})
leftover_add_test(NAME rom_boot COMMAND ${test_rom_boot})

leftover_exe(test_video_beam
    ${MACHINES_ROOT}/tests/apple2/machine/test_video_beam.c
)
target_compile_features(${test_video_beam} PRIVATE c_std_99)
target_link_libraries(${test_video_beam} PRIVATE ${machine})
leftover_add_test(NAME video_beam COMMAND ${test_video_beam})

leftover_exe(test_video_block_paint
    ${MACHINES_ROOT}/tests/apple2/machine/test_video_block_paint.c
)
target_compile_features(${test_video_block_paint} PRIVATE c_std_99)
target_link_libraries(${test_video_block_paint} PRIVATE ${machine})
leftover_add_test(NAME video_block_paint COMMAND ${test_video_block_paint})

leftover_exe(test_video_pixel_address
    ${MACHINES_ROOT}/tests/apple2/machine/test_video_pixel_address.c
)
target_compile_features(${test_video_pixel_address} PRIVATE c_std_99)
target_link_libraries(${test_video_pixel_address} PRIVATE ${machine})
leftover_add_test(NAME video_pixel_address COMMAND ${test_video_pixel_address})

leftover_exe(test_diskii
    ${MACHINES_ROOT}/tests/apple2/machine/test_diskii.c
)
target_compile_features(${test_diskii} PRIVATE c_std_99)
target_compile_definitions(${test_diskii} PRIVATE
    A2M_FIXTURE_DIR="${MACHINES_ROOT}/tests/apple2/fixtures"
)
target_link_libraries(${test_diskii} PRIVATE ${machine})
leftover_add_test(NAME diskii COMMAND ${test_diskii})

leftover_exe(test_peripherals
    ${MACHINES_ROOT}/tests/apple2/machine/test_peripherals.c
)
target_compile_features(${test_peripherals} PRIVATE c_std_99)
target_link_libraries(${test_peripherals} PRIVATE ${machine})
leftover_add_test(NAME peripherals COMMAND ${test_peripherals})

leftover_exe(test_hostfs
    ${MACHINES_ROOT}/tests/apple2/machine/test_hostfs.c
)
target_compile_features(${test_hostfs} PRIVATE c_std_99)
target_compile_definitions(${test_hostfs} PRIVATE
    A2M_FIXTURE_DIR="${MACHINES_ROOT}/tests/apple2/fixtures"
    A2M_SOURCE_DIR="${LEFTOVER_ROOT}"
)
target_link_libraries(${test_hostfs} PRIVATE ${machine})
leftover_add_test(NAME hostfs COMMAND ${test_hostfs})

leftover_exe(test_cxxx_map
    ${MACHINES_ROOT}/tests/apple2/machine/test_cxxx_map.c
)
target_compile_features(${test_cxxx_map} PRIVATE c_std_99)
target_link_libraries(${test_cxxx_map} PRIVATE ${machine})
leftover_add_test(NAME cxxx_map COMMAND ${test_cxxx_map})

leftover_exe(test_memview
    ${MACHINES_ROOT}/tests/apple2/machine/test_memview.c
)
target_compile_features(${test_memview} PRIVATE c_std_99)
target_link_libraries(${test_memview} PRIVATE ${machine})
leftover_add_test(NAME memview COMMAND ${test_memview})

leftover_exe(test_apple2_snapshot
    ${MACHINES_ROOT}/tests/apple2/machine/test_apple2_snapshot.c
)
target_compile_features(${test_apple2_snapshot} PRIVATE c_std_99)
target_link_libraries(${test_apple2_snapshot} PRIVATE ${machine})
leftover_add_test(NAME apple2_snapshot COMMAND ${test_apple2_snapshot})

leftover_exe(bench_realtime
    ${MACHINES_ROOT}/tests/apple2/machine/bench_realtime.c
)
set_target_properties(${bench_realtime} PROPERTIES OUTPUT_NAME bench_realtime)
target_compile_features(${bench_realtime} PRIVATE c_std_99)
target_link_libraries(${bench_realtime} PRIVATE ${machine})

# --- product options / CLI smoke ------------------------------------------

leftover_add_test(NAME a2m_help COMMAND a2m --help)
set_tests_properties(${MACHINES_TEST_NS}.a2m_help PROPERTIES PASS_REGULAR_EXPRESSION "a2m")

leftover_add_test(NAME a2m_version COMMAND a2m --version)
set_tests_properties(${MACHINES_TEST_NS}.a2m_version PROPERTIES PASS_REGULAR_EXPRESSION "a2m 3\\.")

leftover_add_test(NAME a2m_headless COMMAND a2m --noini --headless)
set_tests_properties(${MACHINES_TEST_NS}.a2m_headless PROPERTIES TIMEOUT 15)

leftover_exe(test_app_options_mounts
    ${MACHINES_ROOT}/tests/apple2/test_app_options_mounts.c
    app_options.c
)
target_include_directories(${test_app_options_mounts} PRIVATE
    ${LEFTOVER_ROOT}
    ${LEFTOVER_ROOT}/util
    ${MACHINES_ROOT}/external/argparse
)
target_compile_features(${test_app_options_mounts} PRIVATE c_std_99)
target_compile_definitions(${test_app_options_mounts} PRIVATE
    A2M_FIXTURE_DIR="${MACHINES_ROOT}/tests/apple2/fixtures"
)
target_link_libraries(${test_app_options_mounts} PRIVATE ${util} argparse)
leftover_add_test(NAME app_options_mounts COMMAND ${test_app_options_mounts})

# --- runtime ---------------------------------------------------------------

leftover_exe(test_runtime_stepping
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_stepping.c
)
target_compile_definitions(${test_runtime_stepping} PRIVATE
    A2M_FIXTURE_DIR="${MACHINES_ROOT}/tests/apple2/fixtures"
)
target_compile_features(${test_runtime_stepping} PRIVATE c_std_99)
# util comes via runtime → machine; do not re-list (Apple ld duplicate warning).
target_link_libraries(${test_runtime_stepping} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_stepping COMMAND ${test_runtime_stepping})

leftover_exe(test_runtime_display_stop
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_display_stop.c
)
target_compile_features(${test_runtime_display_stop} PRIVATE c_std_99)
target_link_libraries(${test_runtime_display_stop} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_display_stop COMMAND ${test_runtime_display_stop})

leftover_exe(test_runtime_smartport_boot
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_smartport_boot.c
)
target_compile_features(${test_runtime_smartport_boot} PRIVATE c_std_99)
target_link_libraries(${test_runtime_smartport_boot} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_smartport_boot COMMAND ${test_runtime_smartport_boot})

leftover_exe(test_runtime_step_nested
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_step_nested.c
)
target_compile_features(${test_runtime_step_nested} PRIVATE c_std_99)
target_link_libraries(${test_runtime_step_nested} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_step_nested COMMAND ${test_runtime_step_nested})

leftover_exe(test_runtime_memory_rpc
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_memory_rpc.c
)
target_compile_features(${test_runtime_memory_rpc} PRIVATE c_std_99)
target_link_libraries(${test_runtime_memory_rpc} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_memory_rpc COMMAND ${test_runtime_memory_rpc})

leftover_exe(test_runtime_breakpoint
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_breakpoint.c
)
target_compile_features(${test_runtime_breakpoint} PRIVATE c_std_99)
target_link_libraries(${test_runtime_breakpoint} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_breakpoint COMMAND ${test_runtime_breakpoint})

leftover_exe(test_runtime_breakpoint_ini
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_breakpoint_ini.c
)
target_compile_features(${test_runtime_breakpoint_ini} PRIVATE c_std_99)
target_link_libraries(${test_runtime_breakpoint_ini} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_breakpoint_ini COMMAND ${test_runtime_breakpoint_ini})

leftover_exe(test_runtime_turbo
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_turbo.c
)
target_compile_features(${test_runtime_turbo} PRIVATE c_std_99)
target_link_libraries(${test_runtime_turbo} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_turbo COMMAND ${test_runtime_turbo})

leftover_exe(test_runtime_slot_resolve
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_slot_resolve.c
)
target_compile_features(${test_runtime_slot_resolve} PRIVATE c_std_99)
target_link_libraries(${test_runtime_slot_resolve} PRIVATE ${runtime})
leftover_add_test(NAME runtime_slot_resolve COMMAND ${test_runtime_slot_resolve})

leftover_exe(test_runtime_savestate
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_savestate.c
)
target_compile_features(${test_runtime_savestate} PRIVATE c_std_99)
target_link_libraries(${test_runtime_savestate} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_savestate COMMAND ${test_runtime_savestate})

leftover_exe(test_runtime_machine_files
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_machine_files.c
)
target_compile_features(${test_runtime_machine_files} PRIVATE c_std_99)
target_link_libraries(${test_runtime_machine_files} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_machine_files COMMAND ${test_runtime_machine_files})

# Frame ring unit (ARGB product path).
leftover_exe(test_runtime_frame_ring
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_frame_ring.c
)
target_compile_features(${test_runtime_frame_ring} PRIVATE c_std_99)
target_link_libraries(${test_runtime_frame_ring} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_frame_ring COMMAND ${test_runtime_frame_ring})

# CPU flight recorder basic integration (C3).
leftover_exe(test_runtime_history_basic
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_history_basic.c
)
target_compile_features(${test_runtime_history_basic} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_basic} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_history_basic COMMAND ${test_runtime_history_basic})

# History INFO/RECORD/CLEAR command path (C4a).
leftover_exe(test_runtime_history_commands
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_history_commands.c
)
target_compile_features(${test_runtime_history_commands} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_commands} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_history_commands COMMAND ${test_runtime_history_commands})

# History FIND/READ/CLOSE + HST1 (C4b).
leftover_exe(test_runtime_history_query
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_history_query.c
)
target_compile_features(${test_runtime_history_query} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_query} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_history_query COMMAND ${test_runtime_history_query})

leftover_exe(test_runtime_history
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history.c
)
target_compile_features(${test_runtime_history} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history} PRIVATE shell)
leftover_add_test(NAME runtime_history COMMAND ${test_runtime_history})

leftover_exe(test_runtime_history_wire
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_wire.c
)
target_compile_features(${test_runtime_history_wire} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_wire} PRIVATE shell)
leftover_add_test(NAME runtime_history_wire COMMAND ${test_runtime_history_wire})

leftover_exe(test_runtime_breakpoint_condition
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_breakpoint_condition.c
)
target_compile_features(${test_runtime_breakpoint_condition} PRIVATE c_std_99)
target_link_libraries(${test_runtime_breakpoint_condition} PRIVATE shell)
leftover_add_test(NAME runtime_breakpoint_condition COMMAND ${test_runtime_breakpoint_condition})

# Shared history-find option grammar (Forensics UI / control wire).
leftover_exe(test_runtime_history_query_parse
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_query_parse.c
)
target_compile_features(${test_runtime_history_query_parse} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_query_parse} PRIVATE shell)
leftover_add_test(NAME runtime_history_query_parse COMMAND ${test_runtime_history_query_parse})

# HST1 decode API (round-trip + Python Ctl.decode_hst1 golden).
leftover_exe(test_runtime_history_wire_decode
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_wire_decode.c
)
target_compile_features(${test_runtime_history_wire_decode} PRIVATE c_std_99)
target_compile_definitions(${test_runtime_history_wire_decode} PRIVATE
    HISTORY_WIRE_GOLDEN_PY="${MACHINES_ROOT}/tests/apple2/runtime/check_hst1_decode_golden.py"
)
target_link_libraries(${test_runtime_history_wire_decode} PRIVATE shell)
leftover_add_test(NAME runtime_history_wire_decode COMMAND ${test_runtime_history_wire_decode})

leftover_exe(test_runtime_history_sessions
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_history_sessions.c
)
target_compile_features(${test_runtime_history_sessions} PRIVATE c_std_99)
target_link_libraries(${test_runtime_history_sessions} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_history_sessions COMMAND ${test_runtime_history_sessions})

leftover_exe(test_runtime_state_changed
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_state_changed.c
)
target_compile_features(${test_runtime_state_changed} PRIVATE c_std_99)
target_link_libraries(${test_runtime_state_changed} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_state_changed COMMAND ${test_runtime_state_changed})

leftover_exe(test_runtime_inspector
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_inspector.c
)
target_compile_features(${test_runtime_inspector} PRIVATE c_std_99)
target_link_libraries(${test_runtime_inspector} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_inspector COMMAND ${test_runtime_inspector})

leftover_exe(test_runtime_inspector_replay
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_inspector_replay.c
)
target_compile_features(${test_runtime_inspector_replay} PRIVATE c_std_99)
target_link_libraries(${test_runtime_inspector_replay} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_inspector_replay COMMAND ${test_runtime_inspector_replay})

leftover_exe(test_runtime_inspector_mode
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_inspector_mode.c
)
target_compile_features(${test_runtime_inspector_mode} PRIVATE c_std_99)
target_link_libraries(${test_runtime_inspector_mode} PRIVATE ${control} ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_inspector_mode COMMAND ${test_runtime_inspector_mode})

leftover_exe(test_runtime_inspector_bp
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_inspector_bp.c
)
target_compile_features(${test_runtime_inspector_bp} PRIVATE c_std_99)
target_link_libraries(${test_runtime_inspector_bp} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_inspector_bp COMMAND ${test_runtime_inspector_bp})

# Control protocol (A2M product wire — src/control).
leftover_exe(test_control_protocol
    ${MACHINES_ROOT}/tests/apple2/control/test_control_protocol.c
)
target_compile_features(${test_control_protocol} PRIVATE c_std_99)
target_link_libraries(${test_control_protocol} PRIVATE ${control})
leftover_add_test(NAME control_protocol COMMAND ${test_control_protocol})

leftover_exe(test_control_command_table
    ${MACHINES_ROOT}/tests/shell/control/test_control_command_table.c
)
target_compile_features(${test_control_command_table} PRIVATE c_std_99)
target_link_libraries(${test_control_command_table} PRIVATE shell)
leftover_add_test(NAME control_command_table COMMAND ${test_control_command_table})

leftover_exe(test_memory_source
    ${MACHINES_ROOT}/tests/shell/control/test_memory_source.c
)
target_compile_features(${test_memory_source} PRIVATE c_std_99)
target_link_libraries(${test_memory_source} PRIVATE shell)
leftover_add_test(NAME memory_source COMMAND ${test_memory_source})

leftover_exe(test_runtime_client
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_client.c
)
target_compile_features(${test_runtime_client} PRIVATE c_std_99)
target_link_libraries(${test_runtime_client} PRIVATE shell)
leftover_add_test(NAME runtime_client COMMAND ${test_runtime_client})

# --- tools / assembler -----------------------------------------------------

leftover_exe(test_assembler_expressions
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_expressions.c
)
target_compile_features(${test_assembler_expressions} PRIVATE c_std_99)
target_link_libraries(${test_assembler_expressions} PRIVATE assembler)
leftover_add_test(NAME assembler_expressions COMMAND ${test_assembler_expressions})

leftover_exe(test_assembler_conditionals
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_conditionals.c
)
target_compile_features(${test_assembler_conditionals} PRIVATE c_std_99)
target_link_libraries(${test_assembler_conditionals} PRIVATE assembler)
leftover_add_test(NAME assembler_conditionals COMMAND ${test_assembler_conditionals})

leftover_exe(test_assembler_loops
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_loops.c
)
target_compile_features(${test_assembler_loops} PRIVATE c_std_99)
target_link_libraries(${test_assembler_loops} PRIVATE assembler)
leftover_add_test(NAME assembler_loops COMMAND ${test_assembler_loops})

leftover_exe(test_assembler_macros
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_macros.c
)
target_compile_features(${test_assembler_macros} PRIVATE c_std_99)
target_link_libraries(${test_assembler_macros} PRIVATE assembler)
leftover_add_test(NAME assembler_macros COMMAND ${test_assembler_macros})

leftover_exe(test_assembler_scopes_segments
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_scopes_segments.c
)
target_compile_features(${test_assembler_scopes_segments} PRIVATE c_std_99)
target_link_libraries(${test_assembler_scopes_segments} PRIVATE assembler)
leftover_add_test(NAME assembler_scopes_segments COMMAND ${test_assembler_scopes_segments})

leftover_exe(test_assembler_targets
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_targets.c
)
target_compile_features(${test_assembler_targets} PRIVATE c_std_99)
target_link_libraries(${test_assembler_targets} PRIVATE assembler)
leftover_add_test(NAME assembler_targets COMMAND ${test_assembler_targets})

leftover_exe(test_disasm_6502
    ${MACHINES_ROOT}/tests/shell/tools/test_disasm_6502.c
)
target_compile_features(${test_disasm_6502} PRIVATE c_std_99)
target_link_libraries(${test_disasm_6502} PRIVATE disasm_6502)
leftover_add_test(NAME disasm_6502 COMMAND ${test_disasm_6502})

leftover_exe(test_disasm_6502_65c02
    ${MACHINES_ROOT}/tests/shell/tools/test_disasm_6502_65c02.c
)
target_compile_features(${test_disasm_6502_65c02} PRIVATE c_std_99)
target_link_libraries(${test_disasm_6502_65c02} PRIVATE disasm_6502)
leftover_add_test(NAME disasm_6502_65c02 COMMAND ${test_disasm_6502_65c02})

leftover_exe(test_assembler_65c02
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_65c02.c
)
target_compile_features(${test_assembler_65c02} PRIVATE c_std_99)
target_link_libraries(${test_assembler_65c02} PRIVATE assembler)
leftover_add_test(NAME assembler_65c02 COMMAND ${test_assembler_65c02})

leftover_exe(test_assembler_multifile
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_multifile.c
)
target_compile_features(${test_assembler_multifile} PRIVATE c_std_99)
target_compile_definitions(${test_assembler_multifile} PRIVATE
    A2M_SAMPLE_DIR="${MACHINES_ROOT}/tests/apple2/fixtures/asm"
)
target_link_libraries(${test_assembler_multifile} PRIVATE assembler)
leftover_add_test(NAME assembler_multifile COMMAND ${test_assembler_multifile})

leftover_exe(test_assembler_search
    ${MACHINES_ROOT}/tests/apple2/tools/test_assembler_search.c
)
target_compile_features(${test_assembler_search} PRIVATE c_std_99)
target_link_libraries(${test_assembler_search} PRIVATE assembler)
leftover_add_test(NAME assembler_search COMMAND ${test_assembler_search})

leftover_exe(test_runtime_assembler
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_assembler.c
)
target_compile_features(${test_runtime_assembler} PRIVATE c_std_99)
target_link_libraries(${test_runtime_assembler} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_assembler COMMAND ${test_runtime_assembler})

leftover_exe(test_runtime_assembler_mli
    ${MACHINES_ROOT}/tests/apple2/runtime/test_runtime_assembler_mli.c
)
target_compile_features(${test_runtime_assembler_mli} PRIVATE c_std_99)
target_link_libraries(${test_runtime_assembler_mli} PRIVATE ${runtime} SDL2::SDL2)
leftover_add_test(NAME runtime_assembler_mli COMMAND ${test_runtime_assembler_mli})

leftover_add_test(
    NAME am65_cli_65c02
    COMMAND $<TARGET_FILE:am65> --cpu 65c02
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_65c02.asm
)
leftover_add_test(
    NAME am65_cli_default_rejects_65c02
    COMMAND $<TARGET_FILE:am65>
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_65c02.asm
)
set_tests_properties(${MACHINES_TEST_NS}.am65_cli_default_rejects_65c02 PROPERTIES WILL_FAIL TRUE)
leftover_add_test(
    NAME am65_cli_wdc
    COMMAND $<TARGET_FILE:am65> --cpu wdc
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_wdc.asm
)
leftover_add_test(
    NAME am65_cli_rockwell_rejects_wdc
    COMMAND $<TARGET_FILE:am65> --cpu rockwell
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_wdc.asm
)
set_tests_properties(${MACHINES_TEST_NS}.am65_cli_rockwell_rejects_wdc PROPERTIES WILL_FAIL TRUE)
leftover_add_test(
    NAME am65_cli_search_I
    COMMAND $<TARGET_FILE:am65>
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_search/root.asm
        -I ${MACHINES_ROOT}/tests/apple2/fixtures/am65_search/shared
        -o ${CMAKE_BINARY_DIR}/am65_cli_search.bin
)
leftover_add_test(
    NAME am65_cli_search_without_I_fails
    COMMAND $<TARGET_FILE:am65>
        -i ${MACHINES_ROOT}/tests/apple2/fixtures/am65_search/root.asm
        -o ${CMAKE_BINARY_DIR}/am65_cli_search_fail.bin
)
set_tests_properties(${MACHINES_TEST_NS}.am65_cli_search_without_I_fails PROPERTIES WILL_FAIL TRUE)

# frontend_help.c was replaced by help_view + generated content in the wholesale
# shell; re-add a dedicated help unit test when the public help API stabilizes.

leftover_label_tests()
