# Library-only vendors. No project(). No cmake_minimum_required().
# Unprefixed STATIC targets. PUBLIC includes so #include "log.h" / "ini.h" /
# "stb_ds.h" / "argparse.h" / "re.h" / "whereami.h" resolve.

add_library(stb_ds STATIC stb/stb_ds_impl.c)
target_include_directories(stb_ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_compile_features(stb_ds PUBLIC c_std_99)

add_library(stb_image STATIC stb/stb_image_impl.c)
target_include_directories(stb_image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_compile_features(stb_image PUBLIC c_std_99)

add_library(logc STATIC logc/log.c)
target_include_directories(logc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logc)
target_compile_features(logc PUBLIC c_std_99)
target_compile_definitions(logc PRIVATE LOG_USE_COLOR)

add_library(argparse STATIC argparse/argparse.c)
target_include_directories(argparse PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/argparse)
target_compile_features(argparse PUBLIC c_std_99)

add_library(inih STATIC inih/ini.c)
target_include_directories(inih PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inih)
target_compile_features(inih PUBLIC c_std_99)

add_library(whereami STATIC whereami/whereami.c)
target_include_directories(whereami PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/whereami)
target_compile_features(whereami PUBLIC c_std_99)

add_library(tiny_regex STATIC tiny-regex-c/re.c)
target_include_directories(tiny_regex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tiny-regex-c)
target_compile_features(tiny_regex PUBLIC c_std_99)
