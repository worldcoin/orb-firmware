cmake_minimum_required(VERSION 3.19.0)
set(TARGET app)
# source toolchain
set(CMAKE_TOOLCHAIN_FILE utils/cmake/toolchain-gnuarmemb.cmake)

get_filename_component(WORKSPACE_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
message(STATUS "Workspace directory: ${WORKSPACE_DIR}")

# default board is orb
if (DEFINED ENV{BOARD})
    set(BOARD $ENV{BOARD})
else ()
    set(BOARD orb)
endif ()

# find zephyr directory
# either define environment variable pointing to Zephyr base
# or look for it in the parent directory
if (DEFINED ENV{ZEPHYR_BASE})
    set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})
else()
    set(ZEPHYR_BASE ${WORKSPACE_DIR}/zephyr)
endif()

# Get custom boards definitions from that repo
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-firmware)

target_sources(app PRIVATE src/main.c)
