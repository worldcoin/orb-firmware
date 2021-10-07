cmake_minimum_required(VERSION 3.19.0)
set(TARGET app)

# source toolchain
set(CMAKE_TOOLCHAIN_FILE utils/cmake/toolchain-gnuarmemb.cmake)


# default board is orb
if (DEFINED ENV{BOARD})
    set(BOARD $ENV{BOARD})
else ()
    set(BOARD orb)
endif ()

# Get custom boards definitions from that repo
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-firmware)

set(SOURCES_FILES
        src/main.c

        src/messaging/canbus.c
        src/messaging/messaging.c

        src/power_sequence/power_sequence.c
        src/fan/fan.c
        src/sound/sound.c
        )

# set TEST_TARGET variable to enable the test features
if (TEST_TARGET)
    zephyr_compile_options(-DTEST_TARGET=1)
    list(APPEND SOURCES_FILES src/messaging/tests.c)
endif ()

set(INCLUDE_DIRS
        include
        )

target_sources(${TARGET} PRIVATE ${SOURCES_FILES})
target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})

# link with Zephyr modules below
zephyr_link_libraries(ORB_MCU_LIBRARIES ORB_PROTOBUF_DEFINITIONS_LIB)
