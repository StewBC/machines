# Leftover a2m util (HostFS / disk image helpers). Twins live in src/shell.

if(WIN32)
    set(FS_WATCH_PLATFORM_SOURCE fs_watch_windows.c)
elseif(APPLE)
    set(FS_WATCH_PLATFORM_SOURCE fs_watch_macos.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FS_WATCH_PLATFORM_SOURCE fs_watch_linux.c)
else()
    set(FS_WATCH_PLATFORM_SOURCE fs_watch_unsupported.c)
endif()

add_library(${util} STATIC
    apple2_file.c
    apple_type_script.c
    dynarray.c
    fs_watch.c
    ${FS_WATCH_PLATFORM_SOURCE}
    util_file.c
)

target_include_directories(${util} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(${util} PUBLIC c_std_99)

target_link_libraries(${util}
    PUBLIC
        shell
    PRIVATE
        SDL2::SDL2
)

if(APPLE)
    find_library(CORESERVICES_FRAMEWORK CoreServices REQUIRED)
    target_link_libraries(${util} PRIVATE ${CORESERVICES_FRAMEWORK})
endif()
