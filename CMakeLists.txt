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

# Get our custom boards definitions before loading Zephyr
get_filename_component(WORKSPACE_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(ORB_BOARD_DEF ${WORKSPACE_DIR}/modules/orb-board-def)
list(APPEND BOARD_ROOT ${ORB_BOARD_DEF})
list(APPEND DTS_ROOT ${ORB_BOARD_DEF})

# Add a common dts overlay necessary to ensure the app is linked into,
# and fits inside, the slot0 partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if(DTC_OVERLAY_FILE)
    set(DTC_OVERLAY_FILE
            "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay"
            CACHE STRING "" FORCE
            )
else()
    set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-firmware)

set(SOURCES_FILES
        src/main.c

        src/messaging/canbus.c
        src/messaging/messaging.c
        src/messaging/tests.c

        src/power_sequence/power_sequence.c
        src/fan/fan.c
        src/sound/sound.c
        src/stepper_motors/stepper_motors.c
        )

set(INCLUDE_DIRS
        include
        )

target_sources(${TARGET} PRIVATE ${SOURCES_FILES})
target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})

# link with Zephyr modules below
zephyr_link_libraries(ORB_MCU_LIBRARIES ORB_PROTOBUF_DEFINITIONS_LIB)
