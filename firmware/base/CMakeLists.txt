cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS ../../shared/nibbles_link ../../shared/nibbles_espnow)

project(nibbles_base)

# LVGL draws the screen in software: build it optimised for speed even though the
# rest is built for debugging (-Og). 2026-11-03, for the palette viewer's scrolling.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -O2)
