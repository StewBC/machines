# Leftover a2m platform: AY stereo audio. Host window/fs/socket live in src/shell.

add_library(${platform} STATIC
    platform_audio.c
)

target_include_directories(${platform} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(${platform} PUBLIC c_std_99)

target_link_libraries(${platform}
    PUBLIC
        SDL2::SDL2
        shell
    PRIVATE
        ${util}
)
