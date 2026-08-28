# Leftover c64m util. Twins live in src/shell.

add_library(util STATIC
    basic_v2.c
    paste_parser.c
)

target_include_directories(util PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(util PUBLIC c_std_99)

target_link_libraries(util
    PUBLIC
        shell
)
