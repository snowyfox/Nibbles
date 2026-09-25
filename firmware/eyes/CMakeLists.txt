cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS ./components/bsp_extra ../../shared/nibbles_link ../../shared/nibbles_espnow)

project(nibbles)
