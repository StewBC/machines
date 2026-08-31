# Leftover C64 silicon + c64m executable + c64m tests.
# No project(). No cmake_minimum_required(). Included from repo-root project(machines).

include("${CMAKE_SOURCE_DIR}/cmake/leftover.cmake")

add_subdirectory(util)
add_subdirectory(machine)
add_subdirectory(runtime)
add_subdirectory(platform)
add_subdirectory(control)
add_subdirectory(tools)
add_subdirectory(frontend)

add_executable(c64m
    app_options.c
    main.c
)

target_compile_features(c64m PRIVATE c_std_99)

target_compile_definitions(c64m PRIVATE
    SDL_MAIN_HANDLED
    C64M_VERSION="1.1.0"
)

target_link_libraries(c64m PRIVATE
    argparse
    ${control}
    ${runtime}
    ${platform}
    ${frontend}
    ${tools}
    ${util}
    SDL2::SDL2
)

target_include_directories(c64m PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

leftover_exe(test_audio_buffer
    ${MACHINES_ROOT}/tests/shell/util/test_audio_buffer.c
)

target_compile_features(${test_audio_buffer} PRIVATE c_std_99)

target_link_libraries(${test_audio_buffer} PRIVATE
    ${util}
)

leftover_add_test(NAME audio_buffer COMMAND ${test_audio_buffer})

leftover_exe(test_paste_parser
    ${MACHINES_ROOT}/tests/c64/util/test_paste_parser.c
)

target_compile_features(${test_paste_parser} PRIVATE c_std_99)

target_link_libraries(${test_paste_parser} PRIVATE
    ${util}
)

leftover_add_test(NAME paste_parser COMMAND ${test_paste_parser})

leftover_exe(test_basic_v2
    ${MACHINES_ROOT}/tests/c64/util/test_basic_v2.c
)

target_compile_features(${test_basic_v2} PRIVATE c_std_99)

target_link_libraries(${test_basic_v2} PRIVATE
    ${util}
)

leftover_add_test(NAME basic_v2 COMMAND ${test_basic_v2})

leftover_exe(test_c64_bus
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_bus.c
)

target_compile_features(${test_c64_bus} PRIVATE c_std_99)

target_link_libraries(${test_c64_bus} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_bus COMMAND ${test_c64_bus})

leftover_exe(test_c64_snapshot
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_snapshot.c
)

target_compile_features(${test_c64_snapshot} PRIVATE c_std_99)

target_compile_definitions(${test_c64_snapshot} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
    C64M_TEST_DIR="${MACHINES_ROOT}/tests/c64"
)

target_link_libraries(${test_c64_snapshot} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_snapshot COMMAND ${test_c64_snapshot})

leftover_exe(test_c64_snapshot_1541_midload
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_snapshot_1541_midload.c
)

target_compile_features(${test_c64_snapshot_1541_midload} PRIVATE c_std_99)

target_compile_definitions(${test_c64_snapshot_1541_midload} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_c64_snapshot_1541_midload} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_snapshot_1541_midload COMMAND ${test_c64_snapshot_1541_midload})
set_tests_properties(${MACHINES_TEST_NS}.c64_snapshot_1541_midload PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_sid
    ${MACHINES_ROOT}/tests/c64/machine/test_sid.c
)

target_compile_features(${test_sid} PRIVATE c_std_99)

target_link_libraries(${test_sid} PRIVATE
    ${machine}
    ${util}
)

if(NOT WIN32)
    target_link_libraries(${test_sid} PRIVATE m)
endif()

leftover_add_test(NAME sid COMMAND ${test_sid})

leftover_exe(test_via6522
    ${MACHINES_ROOT}/tests/c64/machine/test_via6522.c
)

target_compile_features(${test_via6522} PRIVATE c_std_99)

target_link_libraries(${test_via6522} PRIVATE
    ${machine}
)

leftover_add_test(NAME via6522 COMMAND ${test_via6522})

leftover_exe(test_c1541
    ${MACHINES_ROOT}/tests/c64/machine/test_c1541.c
)

target_compile_features(${test_c1541} PRIVATE c_std_99)

target_link_libraries(${test_c1541} PRIVATE
    ${machine}
)

leftover_add_test(NAME c1541 COMMAND ${test_c1541})

leftover_exe(test_c64_hostfs_mount
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_hostfs_mount.c
)

target_compile_features(${test_c64_hostfs_mount} PRIVATE c_std_99)

target_link_libraries(${test_c64_hostfs_mount} PRIVATE
    ${machine}
    ${d64}
)

leftover_add_test(NAME c64_hostfs_mount COMMAND ${test_c64_hostfs_mount})

leftover_exe(test_c1541_gcr
    ${MACHINES_ROOT}/tests/c64/machine/test_c1541_gcr.c
)

target_compile_features(${test_c1541_gcr} PRIVATE c_std_99)

target_link_libraries(${test_c1541_gcr} PRIVATE
    ${machine}
)

leftover_add_test(NAME c1541_gcr COMMAND ${test_c1541_gcr})

leftover_exe(test_g64
    ${MACHINES_ROOT}/tests/c64/tools/test_g64.c
)

target_compile_features(${test_g64} PRIVATE c_std_99)

target_link_libraries(${test_g64} PRIVATE
    ${g64}
)

leftover_add_test(NAME g64 COMMAND ${test_g64})

leftover_exe(test_c1541_media
    ${MACHINES_ROOT}/tests/c64/machine/test_c1541_media.c
)

target_compile_features(${test_c1541_media} PRIVATE c_std_99)

target_link_libraries(${test_c1541_media} PRIVATE
    ${machine}
)

leftover_add_test(NAME c1541_media COMMAND ${test_c1541_media})

leftover_exe(test_c64_rom
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_rom.c
)

target_compile_features(${test_c64_rom} PRIVATE c_std_99)

target_link_libraries(${test_c64_rom} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_rom COMMAND ${test_c64_rom})

leftover_exe(test_c64_frame
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_frame.c
)

target_compile_features(${test_c64_frame} PRIVATE c_std_99)

target_link_libraries(${test_c64_frame} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_frame COMMAND ${test_c64_frame})

leftover_exe(test_c64_vicii
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_vicii.c
)

target_compile_features(${test_c64_vicii} PRIVATE c_std_99)

target_link_libraries(${test_c64_vicii} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_vicii COMMAND ${test_c64_vicii})

leftover_exe(test_c64_cia
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_cia.c
)

target_compile_features(${test_c64_cia} PRIVATE c_std_99)

target_link_libraries(${test_c64_cia} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_cia COMMAND ${test_c64_cia})

leftover_exe(test_c64_boot_progression
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_boot_progression.c
)

target_compile_features(${test_c64_boot_progression} PRIVATE c_std_99)

target_compile_definitions(${test_c64_boot_progression} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_c64_boot_progression} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_boot_progression COMMAND ${test_c64_boot_progression})

leftover_exe(test_c64_keyboard
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_keyboard.c
)

target_compile_features(${test_c64_keyboard} PRIVATE c_std_99)

target_link_libraries(${test_c64_keyboard} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_keyboard COMMAND ${test_c64_keyboard})

leftover_exe(test_c64_disk_load
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_disk_load.c
)

target_compile_features(${test_c64_disk_load} PRIVATE c_std_99)

target_compile_definitions(${test_c64_disk_load} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_c64_disk_load} PRIVATE
    ${machine}
    ${d64}
)

leftover_add_test(NAME c64_disk_load COMMAND ${test_c64_disk_load})
set_tests_properties(${MACHINES_TEST_NS}.c64_disk_load PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_c64_real_1541_load
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_real_1541_load.c
)

target_compile_features(${test_c64_real_1541_load} PRIVATE c_std_99)

target_compile_definitions(${test_c64_real_1541_load} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_c64_real_1541_load} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_real_1541_load COMMAND ${test_c64_real_1541_load})
set_tests_properties(${MACHINES_TEST_NS}.c64_real_1541_load PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_c64_robocop_g64
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_robocop_g64.c
)

target_compile_features(${test_c64_robocop_g64} PRIVATE c_std_99)

target_compile_definitions(${test_c64_robocop_g64} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_c64_robocop_g64} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_robocop_g64 COMMAND ${test_c64_robocop_g64})
set_tests_properties(${MACHINES_TEST_NS}.c64_robocop_g64 PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(probe_c64_arkanoid_g64
    ${MACHINES_ROOT}/tests/c64/machine/probe_c64_arkanoid_g64.c
)

target_compile_features(${probe_c64_arkanoid_g64} PRIVATE c_std_99)

target_compile_definitions(${probe_c64_arkanoid_g64} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${probe_c64_arkanoid_g64} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_arkanoid_g64 COMMAND ${probe_c64_arkanoid_g64})
leftover_add_test(NAME c64_arkanoid_alt_g64 COMMAND ${probe_c64_arkanoid_g64} alt)
set_tests_properties(${MACHINES_TEST_NS}.c64_arkanoid_g64 ${MACHINES_TEST_NS}.c64_arkanoid_alt_g64 PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_frontend_input
    ${MACHINES_ROOT}/tests/c64/frontend/test_frontend_input.c
)

target_compile_features(${test_frontend_input} PRIVATE c_std_99)

target_compile_definitions(${test_frontend_input} PRIVATE SDL_MAIN_HANDLED)

target_include_directories(${test_frontend_input} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/machine
)

target_link_libraries(${test_frontend_input} PRIVATE
    ${frontend}
)

leftover_add_test(NAME frontend_input COMMAND ${test_frontend_input})

leftover_exe(test_help_view
    ${MACHINES_ROOT}/tests/shell/frontend/test_help_view.c
)

target_compile_features(${test_help_view} PRIVATE c_std_99)

target_compile_definitions(${test_help_view} PRIVATE SDL_MAIN_HANDLED)

target_include_directories(${test_help_view} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/machine
)

target_link_libraries(${test_help_view} PRIVATE
    ${frontend}
)

leftover_add_test(NAME help_view COMMAND ${test_help_view})

leftover_exe(test_forensics_view
    ${MACHINES_ROOT}/tests/shell/frontend/test_forensics_view.c
)

target_compile_features(${test_forensics_view} PRIVATE c_std_99)

target_compile_definitions(${test_forensics_view} PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(${test_forensics_view} PRIVATE
    shell
)

leftover_add_test(NAME forensics_view COMMAND ${test_forensics_view})

leftover_exe(test_frontend_joystick
    ${MACHINES_ROOT}/tests/c64/frontend/test_frontend_joystick.c
)

target_compile_features(${test_frontend_joystick} PRIVATE c_std_99)

target_compile_definitions(${test_frontend_joystick} PRIVATE SDL_MAIN_HANDLED)

target_include_directories(${test_frontend_joystick} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
)

target_link_libraries(${test_frontend_joystick} PRIVATE
    ${frontend}
)

leftover_add_test(NAME frontend_joystick COMMAND ${test_frontend_joystick})

leftover_exe(test_window_title
    frontend/window_title.c
    ${MACHINES_ROOT}/src/shell/frontend/window_title.c
    ${MACHINES_ROOT}/tests/c64/frontend/test_window_title.c
)

target_compile_features(${test_window_title} PRIVATE c_std_99)

target_include_directories(${test_window_title} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/machine
    ${MACHINES_ROOT}/src/shell/runtime
    ${MACHINES_ROOT}/src/shell/frontend
)

leftover_add_test(NAME window_title COMMAND ${test_window_title})

leftover_exe(test_window_title_shell
    ${MACHINES_ROOT}/tests/shell/frontend/test_window_title.c
    ${MACHINES_ROOT}/src/shell/frontend/window_title.c
)
target_include_directories(${test_window_title_shell} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)
target_compile_features(${test_window_title_shell} PRIVATE c_std_99)
leftover_add_test(NAME window_title_shell COMMAND ${test_window_title_shell})

leftover_exe(test_memory_search
    ${MACHINES_ROOT}/tests/shell/frontend/test_memory_search.c
    ${MACHINES_ROOT}/src/shell/frontend/memory_search.c
)
target_include_directories(${test_memory_search} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)
target_compile_features(${test_memory_search} PRIVATE c_std_99)
leftover_add_test(NAME memory_search COMMAND ${test_memory_search})

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

leftover_exe(test_c64_memview_sources
    ${MACHINES_ROOT}/tests/c64/frontend/test_c64_memview_sources.c
)
target_compile_features(${test_c64_memview_sources} PRIVATE c_std_99)
target_link_libraries(${test_c64_memview_sources} PRIVATE ${control} shell)
leftover_add_test(NAME c64_memview_sources COMMAND ${test_c64_memview_sources})

leftover_exe(test_crt_renderer
    frontend/crt_renderer.c
    ${MACHINES_ROOT}/tests/c64/frontend/test_crt_renderer.c
)

target_compile_features(${test_crt_renderer} PRIVATE c_std_99)

target_include_directories(${test_crt_renderer} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/frontend
)

leftover_add_test(NAME crt_renderer COMMAND ${test_crt_renderer})

leftover_exe(test_platform_fs
    ${MACHINES_ROOT}/tests/shell/platform/test_platform_fs.c
)

target_compile_features(${test_platform_fs} PRIVATE c_std_99)

target_include_directories(${test_platform_fs} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
)

target_link_libraries(${test_platform_fs} PRIVATE
    ${platform}
)

leftover_add_test(NAME platform_fs COMMAND ${test_platform_fs})

leftover_exe(test_c64_cpu_validation
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_cpu_validation.c
)

target_compile_features(${test_c64_cpu_validation} PRIVATE c_std_99)

target_link_libraries(${test_c64_cpu_validation} PRIVATE
    ${runtime}
)

leftover_add_test(NAME c64_cpu_validation COMMAND ${test_c64_cpu_validation})

leftover_exe(test_c64_cpu_observer
    ${MACHINES_ROOT}/tests/c64/machine/test_c64_cpu_observer.c
)

target_compile_features(${test_c64_cpu_observer} PRIVATE c_std_99)

target_link_libraries(${test_c64_cpu_observer} PRIVATE
    ${machine}
)

leftover_add_test(NAME c64_cpu_observer COMMAND ${test_c64_cpu_observer})

leftover_exe(test_app_options
    app_options.c
    ${MACHINES_ROOT}/tests/c64/test_app_options.c
)

target_compile_features(${test_app_options} PRIVATE c_std_99)

target_include_directories(${test_app_options} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${test_app_options} PRIVATE
    argparse
    ${util}
)

leftover_add_test(NAME app_options COMMAND ${test_app_options})

leftover_exe(test_control_protocol
    ${MACHINES_ROOT}/tests/c64/control/test_control_protocol.c
)

target_compile_features(${test_control_protocol} PRIVATE c_std_99)

target_link_libraries(${test_control_protocol} PRIVATE
    ${control}
)

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

leftover_exe(test_runtime_symbol_snapshot
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_symbol_snapshot.c
)
target_compile_features(${test_runtime_symbol_snapshot} PRIVATE c_std_99)
target_link_libraries(${test_runtime_symbol_snapshot} PRIVATE shell)
leftover_add_test(NAME runtime_symbol_snapshot COMMAND ${test_runtime_symbol_snapshot})

leftover_add_test(
    NAME history_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_history_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.history_control_integration PROPERTIES
    TIMEOUT 20
)

leftover_add_test(
    NAME run_to_raster_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_run_to_raster_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.run_to_raster_control_integration PROPERTIES
    TIMEOUT 30
)

leftover_add_test(
    NAME guarded_breakpoint_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_guarded_breakpoint_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.guarded_breakpoint_control_integration PROPERTIES
    TIMEOUT 60
)

leftover_add_test(
    NAME frame_ring_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_frame_ring_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.frame_ring_control_integration PROPERTIES
    TIMEOUT 60
)

leftover_add_test(
    NAME vic_ring_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_vic_ring_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.vic_ring_control_integration PROPERTIES
    TIMEOUT 60
)

leftover_add_test(
    NAME inspector_control_integration
    COMMAND
        ${Python3_EXECUTABLE}
        ${MACHINES_ROOT}/tests/c64/control/test_inspector_control.py
        $<TARGET_FILE:c64m>
        ${MACHINES_ROOT}
)

set_tests_properties(${MACHINES_TEST_NS}.inspector_control_integration PROPERTIES
    TIMEOUT 30
)

leftover_exe(test_disasm_pc_lock
    ${MACHINES_ROOT}/tests/shell/frontend/test_disasm_pc_lock.c
    ${MACHINES_ROOT}/src/shell/frontend/disasm_pc_lock.c
)

target_compile_features(${test_disasm_pc_lock} PRIVATE c_std_99)

target_include_directories(${test_disasm_pc_lock} PRIVATE
    ${MACHINES_ROOT}/src/shell/frontend
)

target_link_libraries(${test_disasm_pc_lock} PRIVATE
    disasm_6502
)

leftover_add_test(NAME disasm_pc_lock COMMAND ${test_disasm_pc_lock})

leftover_exe(test_disasm_6502
    ${MACHINES_ROOT}/tests/shell/tools/test_disasm_6502.c
)

target_compile_features(${test_disasm_6502} PRIVATE c_std_99)

target_link_libraries(${test_disasm_6502} PRIVATE
    disasm_6502
)

leftover_add_test(NAME disasm_6502 COMMAND ${test_disasm_6502})

leftover_exe(test_symbol_table
    ${MACHINES_ROOT}/tests/c64/tools/test_symbol_table.c
)

target_compile_features(${test_symbol_table} PRIVATE c_std_99)

target_link_libraries(${test_symbol_table} PRIVATE
    symbols
)

leftover_add_test(NAME symbol_table COMMAND ${test_symbol_table})

leftover_exe(test_d64
    ${MACHINES_ROOT}/tests/c64/tools/test_d64.c
)

target_compile_features(${test_d64} PRIVATE c_std_99)

target_compile_definitions(${test_d64} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_d64} PRIVATE
    ${d64}
)

leftover_add_test(NAME d64 COMMAND ${test_d64})
set_tests_properties(${MACHINES_TEST_NS}.d64 PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_t64
    ${MACHINES_ROOT}/tests/c64/tools/test_t64.c
)

target_compile_features(${test_t64} PRIVATE c_std_99)

target_compile_definitions(${test_t64} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_t64} PRIVATE
    ${t64}
)

leftover_add_test(NAME t64 COMMAND ${test_t64})
set_tests_properties(${MACHINES_TEST_NS}.t64 PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_crt
    ${MACHINES_ROOT}/tests/c64/tools/test_crt.c
)

target_compile_features(${test_crt} PRIVATE c_std_99)

target_link_libraries(${test_crt} PRIVATE
    ${crt}
)

leftover_add_test(NAME crt COMMAND ${test_crt})

leftover_exe(profile_c64_hotloop
    ${MACHINES_ROOT}/tools/c64/profile_c64_hotloop.c
)
set_target_properties(${profile_c64_hotloop} PROPERTIES OUTPUT_NAME profile_c64_hotloop)

target_compile_features(${profile_c64_hotloop} PRIVATE c_std_99)

target_compile_definitions(${profile_c64_hotloop} PRIVATE
    MACHINES_ROMS_DIR="${MACHINES_ROOT}/roms"
)

target_link_libraries(${profile_c64_hotloop} PRIVATE
    ${machine}
)

leftover_exe(run_c64m_cia_corpus
    ${MACHINES_ROOT}/tools/c64/cia-timing-corpus/run_c64m.c
)
set_target_properties(${run_c64m_cia_corpus} PROPERTIES OUTPUT_NAME run_c64m_cia_corpus)

target_compile_features(${run_c64m_cia_corpus} PRIVATE c_std_99)

target_compile_definitions(${run_c64m_cia_corpus} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
    MACHINES_ROMS_DIR="${MACHINES_ROOT}/roms"
)

target_link_libraries(${run_c64m_cia_corpus} PRIVATE
    ${machine}
)

leftover_exe(profile_runtime_hotloop
    ${MACHINES_ROOT}/tools/c64/profile_runtime_hotloop.c
)
set_target_properties(${profile_runtime_hotloop} PROPERTIES OUTPUT_NAME profile_runtime_hotloop)

target_compile_features(${profile_runtime_hotloop} PRIVATE c_std_99)

target_compile_definitions(${profile_runtime_hotloop} PRIVATE
    MACHINES_ROMS_DIR="${MACHINES_ROOT}/roms"
)

target_link_libraries(${profile_runtime_hotloop} PRIVATE
    ${runtime}
)

leftover_exe(profile_history_query
    ${MACHINES_ROOT}/tools/c64/profile_history_query.c
)
set_target_properties(${profile_history_query} PROPERTIES OUTPUT_NAME profile_history_query)

target_compile_features(${profile_history_query} PRIVATE c_std_99)

target_link_libraries(${profile_history_query} PRIVATE
    ${runtime}
)

leftover_exe(eod_regression_capture
    ${MACHINES_ROOT}/tools/c64/eod_regression_capture.c
)
set_target_properties(${eod_regression_capture} PROPERTIES OUTPUT_NAME eod_regression_capture)

target_compile_features(${eod_regression_capture} PRIVATE c_std_99)

target_include_directories(${eod_regression_capture} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)

target_link_libraries(${eod_regression_capture} PRIVATE
    ${runtime}
)

leftover_exe(test_assembler_conditionals
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_conditionals.c
)

target_compile_features(${test_assembler_conditionals} PRIVATE c_std_99)

target_link_libraries(${test_assembler_conditionals} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_conditionals COMMAND ${test_assembler_conditionals})

leftover_exe(test_assembler_expressions
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_expressions.c
)

target_compile_features(${test_assembler_expressions} PRIVATE c_std_99)

target_link_libraries(${test_assembler_expressions} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_expressions COMMAND ${test_assembler_expressions})

# Standing opcode/addressing-mode coverage matrix. Built with the tree so it
# cannot bitrot, but intentionally NOT registered with leftover_add_test(): it is a
# manual tool for assembler work, not a CI gate. To promote it to a gate, add:
#   leftover_add_test(NAME assembler_opcode_matrix COMMAND ${test_assembler_opcode_matrix})
leftover_exe(test_assembler_opcode_matrix
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_opcode_matrix.c
)
set_target_properties(${test_assembler_opcode_matrix} PROPERTIES OUTPUT_NAME test_assembler_opcode_matrix)

target_compile_features(${test_assembler_opcode_matrix} PRIVATE c_std_99)

target_link_libraries(${test_assembler_opcode_matrix} PRIVATE
    assembler
    disasm_6502
)

leftover_exe(test_assembler_targets
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_targets.c
)

target_compile_features(${test_assembler_targets} PRIVATE c_std_99)

target_link_libraries(${test_assembler_targets} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_targets COMMAND ${test_assembler_targets})

leftover_exe(test_assembler_loops
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_loops.c
)

target_compile_features(${test_assembler_loops} PRIVATE c_std_99)

target_link_libraries(${test_assembler_loops} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_loops COMMAND ${test_assembler_loops})

leftover_exe(test_assembler_macros
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_macros.c
)

target_compile_features(${test_assembler_macros} PRIVATE c_std_99)

target_link_libraries(${test_assembler_macros} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_macros COMMAND ${test_assembler_macros})

leftover_exe(test_assembler_scopes_segments
    ${MACHINES_ROOT}/tests/c64/tools/test_assembler_scopes_segments.c
)

target_compile_features(${test_assembler_scopes_segments} PRIVATE c_std_99)

target_link_libraries(${test_assembler_scopes_segments} PRIVATE
    assembler
)

leftover_add_test(NAME assembler_scopes_segments COMMAND ${test_assembler_scopes_segments})

leftover_exe(test_runtime_romboot
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_romboot.c
)

target_compile_features(${test_runtime_romboot} PRIVATE c_std_99)

target_link_libraries(${test_runtime_romboot} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_romboot COMMAND ${test_runtime_romboot})

leftover_exe(test_runtime_scheduler
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_scheduler.c
)

target_compile_features(${test_runtime_scheduler} PRIVATE c_std_99)

target_link_libraries(${test_runtime_scheduler} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_scheduler COMMAND ${test_runtime_scheduler})

leftover_exe(test_runtime_frame
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_frame.c
)

target_compile_features(${test_runtime_frame} PRIVATE c_std_99)

target_link_libraries(${test_runtime_frame} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_frame COMMAND ${test_runtime_frame})

leftover_exe(test_runtime_disk
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_disk.c
)

target_compile_features(${test_runtime_disk} PRIVATE c_std_99)

target_compile_definitions(${test_runtime_disk} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_runtime_disk} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_disk COMMAND ${test_runtime_disk})
set_tests_properties(${MACHINES_TEST_NS}.runtime_disk PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_runtime_crt
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_crt.c
)

target_compile_features(${test_runtime_crt} PRIVATE c_std_99)

target_link_libraries(${test_runtime_crt} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_crt COMMAND ${test_runtime_crt})

leftover_exe(test_runtime_real_1541_autorun
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_real_1541_autorun.c
)

target_compile_features(${test_runtime_real_1541_autorun} PRIVATE c_std_99)

target_compile_definitions(${test_runtime_real_1541_autorun} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_runtime_real_1541_autorun} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_real_1541_autorun COMMAND ${test_runtime_real_1541_autorun})
set_tests_properties(${MACHINES_TEST_NS}.runtime_real_1541_autorun PROPERTIES SKIP_RETURN_CODE 77)

leftover_exe(test_runtime_basic_run
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_basic_run.c
)

target_compile_features(${test_runtime_basic_run} PRIVATE c_std_99)

target_compile_definitions(${test_runtime_basic_run} PRIVATE
    C64M_SOURCE_DIR="${MACHINES_ROOT}"
)

target_link_libraries(${test_runtime_basic_run} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_basic_run COMMAND ${test_runtime_basic_run})

leftover_exe(test_runtime_assembler
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_assembler.c
)

target_compile_features(${test_runtime_assembler} PRIVATE c_std_99)

target_link_libraries(${test_runtime_assembler} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_assembler COMMAND ${test_runtime_assembler})

leftover_exe(test_runtime_fileio
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_fileio.c
)

target_compile_features(${test_runtime_fileio} PRIVATE c_std_99)

target_link_libraries(${test_runtime_fileio} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_fileio COMMAND ${test_runtime_fileio})

leftover_exe(test_runtime_savestate
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_savestate.c
)

target_compile_features(${test_runtime_savestate} PRIVATE c_std_99)

target_link_libraries(${test_runtime_savestate} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_savestate COMMAND ${test_runtime_savestate})

leftover_exe(test_runtime_stepping
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_stepping.c
)

target_compile_features(${test_runtime_stepping} PRIVATE c_std_99)

target_link_libraries(${test_runtime_stepping} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_stepping COMMAND ${test_runtime_stepping})

leftover_exe(test_runtime_request_token
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_request_token.c
)

target_compile_features(${test_runtime_request_token} PRIVATE c_std_99)

target_link_libraries(${test_runtime_request_token} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_request_token COMMAND ${test_runtime_request_token})

leftover_exe(test_runtime_memory_rpc
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_memory_rpc.c
)

target_compile_features(${test_runtime_memory_rpc} PRIVATE c_std_99)

target_link_libraries(${test_runtime_memory_rpc} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_memory_rpc COMMAND ${test_runtime_memory_rpc})

leftover_exe(test_runtime_run_to_raster
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_run_to_raster.c
)

target_compile_features(${test_runtime_run_to_raster} PRIVATE c_std_99)

target_link_libraries(${test_runtime_run_to_raster} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_run_to_raster COMMAND ${test_runtime_run_to_raster})

leftover_exe(test_runtime_flight_recorder
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_flight_recorder.c
)

target_compile_features(${test_runtime_flight_recorder} PRIVATE c_std_99)

target_link_libraries(${test_runtime_flight_recorder} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_flight_recorder COMMAND ${test_runtime_flight_recorder})

leftover_exe(test_runtime_breakpoint_condition
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_breakpoint_condition.c
)

target_compile_features(${test_runtime_breakpoint_condition} PRIVATE c_std_99)

target_link_libraries(${test_runtime_breakpoint_condition} PRIVATE
    shell
)

leftover_add_test(NAME runtime_breakpoint_condition COMMAND ${test_runtime_breakpoint_condition})

leftover_exe(test_runtime_breakpoint_ini
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_breakpoint_ini.c
)

target_compile_features(${test_runtime_breakpoint_ini} PRIVATE c_std_99)

target_link_libraries(${test_runtime_breakpoint_ini} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_breakpoint_ini COMMAND ${test_runtime_breakpoint_ini})

leftover_exe(test_runtime_frame_ring
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_frame_ring.c
)

target_compile_features(${test_runtime_frame_ring} PRIVATE c_std_99)

target_link_libraries(${test_runtime_frame_ring} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_frame_ring COMMAND ${test_runtime_frame_ring})

leftover_exe(test_runtime_inspector
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_inspector.c
)

target_compile_features(${test_runtime_inspector} PRIVATE c_std_99)

target_link_libraries(${test_runtime_inspector} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_inspector COMMAND ${test_runtime_inspector})

leftover_exe(test_runtime_inspector_replay
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_inspector_replay.c
)

target_compile_features(${test_runtime_inspector_replay} PRIVATE c_std_99)

target_link_libraries(${test_runtime_inspector_replay} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_inspector_replay COMMAND ${test_runtime_inspector_replay})

leftover_exe(test_runtime_inspector_mode
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_inspector_mode.c
)

target_compile_features(${test_runtime_inspector_mode} PRIVATE c_std_99)

target_link_libraries(${test_runtime_inspector_mode} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_inspector_mode COMMAND ${test_runtime_inspector_mode})

leftover_exe(test_runtime_vic_ring
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_vic_ring.c
)

target_compile_features(${test_runtime_vic_ring} PRIVATE c_std_99)

target_link_libraries(${test_runtime_vic_ring} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_vic_ring COMMAND ${test_runtime_vic_ring})

leftover_exe(test_runtime_history
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history.c
)

target_compile_features(${test_runtime_history} PRIVATE c_std_99)

target_link_libraries(${test_runtime_history} PRIVATE
    shell
)

leftover_add_test(NAME runtime_history COMMAND ${test_runtime_history})

leftover_exe(test_runtime_history_wire
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_wire.c
)

target_compile_features(${test_runtime_history_wire} PRIVATE c_std_99)

target_link_libraries(${test_runtime_history_wire} PRIVATE
    shell
)

leftover_add_test(NAME runtime_history_wire COMMAND ${test_runtime_history_wire})

leftover_exe(test_runtime_history_wire_decode
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_wire_decode.c
)

target_compile_features(${test_runtime_history_wire_decode} PRIVATE c_std_99)

target_compile_definitions(${test_runtime_history_wire_decode} PRIVATE
    HISTORY_WIRE_GOLDEN_PY="${MACHINES_ROOT}/tests/c64/runtime/check_hst1_decode_golden.py"
)

target_link_libraries(${test_runtime_history_wire_decode} PRIVATE
    shell
)

leftover_add_test(NAME runtime_history_wire_decode COMMAND ${test_runtime_history_wire_decode})

leftover_exe(test_runtime_history_query_parse
    ${MACHINES_ROOT}/tests/shell/runtime/test_runtime_history_query_parse.c
)

target_compile_features(${test_runtime_history_query_parse} PRIVATE c_std_99)

target_link_libraries(${test_runtime_history_query_parse} PRIVATE
    shell
)

leftover_add_test(NAME runtime_history_query_parse COMMAND ${test_runtime_history_query_parse})

leftover_exe(test_runtime_history_sessions
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_history_sessions.c
)

target_compile_features(${test_runtime_history_sessions} PRIVATE c_std_99)

target_link_libraries(${test_runtime_history_sessions} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_history_sessions COMMAND ${test_runtime_history_sessions})

leftover_exe(test_runtime_state_changed
    ${MACHINES_ROOT}/tests/c64/runtime/test_runtime_state_changed.c
)

target_compile_features(${test_runtime_state_changed} PRIVATE c_std_99)

target_link_libraries(${test_runtime_state_changed} PRIVATE
    ${runtime}
)

leftover_add_test(NAME runtime_state_changed COMMAND ${test_runtime_state_changed})

leftover_label_tests()
