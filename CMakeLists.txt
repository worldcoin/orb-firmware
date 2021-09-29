cmake_minimum_required(VERSION 3.20.0)

# source toolchain
set(CMAKE_TOOLCHAIN_FILE utils/cmake/toolchain-gnuarmemb.cmake)

# default board is stm32g484_eval
set(BOARD stm32g484_eval)

# find zephyr directory
# either define environment variable pointing to Zephyr base
# or look for it in the parent directory
if (DEFINED ENV{ZEPHYR_BASE})
    set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})
else()
    get_filename_component(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
    set(ZEPHYR_BASE ${ROOT_DIR}/zephyr)
endif()

# Get custom boards definitions from that repo
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE src/main.c)
