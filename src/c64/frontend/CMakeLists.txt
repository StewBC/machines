# src/frontend/CMakeLists.txt

set(C64M_HELP_CONTENT "${CMAKE_CURRENT_BINARY_DIR}/generated/help_content.inc")
set(C64M_HELP_SECTION_LEVEL "2" CACHE STRING "Markdown heading level used for help sections")

add_custom_command(
    OUTPUT "${C64M_HELP_CONTENT}"
    COMMAND Python3::Interpreter
        "${MACHINES_ROOT}/src/shell/tools/gen_help.py"
        --level "${C64M_HELP_SECTION_LEVEL}"
        "${MACHINES_ROOT}/manual/c64m/manual.md"
        "${C64M_HELP_CONTENT}"
    DEPENDS
        "${MACHINES_ROOT}/manual/c64m/manual.md"
        "${MACHINES_ROOT}/src/shell/tools/gen_help.py"
    VERBATIM
)

add_custom_target(${MACHINES_LEFTOVER}_frontend_help_content DEPENDS "${C64M_HELP_CONTENT}")

add_library(${frontend} STATIC
    crt_renderer.c
    crt_renderer.h
    frontend.c
    frontend.h
    frontend_input.c
    frontend_input.h
    frontend_joystick_input.c
    frontend_joystick_input.h
    frontend_mouse_input.c
    frontend_mouse_input.h
    ${MACHINES_ROOT}/src/shell/frontend/help_view.c
    window_title.c
    window_title.h
    "${C64M_HELP_CONTENT}"
)

add_dependencies(${frontend} ${MACHINES_LEFTOVER}_frontend_help_content)

target_include_directories(${frontend} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LEFTOVER_ROOT}
    ${SDL2_INCLUDE_DIRS}
)

target_compile_features(${frontend} PUBLIC c_std_99)

if(MSVC)
    # nuklear.h/nuklear_sdl.h are vendored third-party headers; these MSVC
    # diagnostics fire on intentional, standard-legal idioms there (the
    # pre-C11 alignof-emulation trick, and enum-family flag combinations)
    # that we don't want to alter upstream.
    target_compile_options(${frontend} PRIVATE /wd4116 /wd5287)
endif()

target_link_libraries(${frontend}
    PUBLIC
        ${runtime}
        shell
    PRIVATE
        tiny_regex
        stb_image
        ${platform}
        SDL2::SDL2
        ${tools}
        ${util}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${frontend} PRIVATE m)
endif()
