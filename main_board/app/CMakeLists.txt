cmake_minimum_required(VERSION 3.20.0)
set(TARGET app)

set_property(GLOBAL PROPERTY CSTD gnu11)

# default board is orb
if (DEFINED ENV{BOARD})
    set(BOARD $ENV{BOARD})
else ()
    set(BOARD mcu_main)
endif ()

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../)

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
    set(DTC_OVERLAY_FILE dts.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-mcu-main-app)

set(SOURCES_FILES
        src/main.c

        src/messaging/canbus.c
        src/messaging/messaging.c
        src/messaging/incoming_message_handling.c
        src/messaging/messaging_tests.c
        )

target_sources_ifdef(CONFIG_BOARD_MCU_MAIN ${TARGET} PRIVATE
        src/power_sequence/power_sequence.c
        src/fan/fan.c
        src/sound/sound.c

        src/front_unit_rgb_leds/front_unit_rgb_leds.c
        src/front_unit_rgb_leds/front_unit_led_patterns.c

        src/distributor_leds/distributor_leds.c

        src/ir_camera_system/ir_camera_system.c
        src/ir_camera_system/ir_camera_timer_settings.c
        src/ir_camera_system/ir_camera_system_test.c

        src/stepper_motors/stepper_motors.c
        src/stepper_motors/motors_tests.c

        src/temperature/temperature.c

        )

set(INCLUDE_DIRS
        include
        src
        )

target_sources(${TARGET} PRIVATE ${SOURCES_FILES})
target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})

# link with Zephyr modules below
zephyr_link_libraries(ORB_PROTOBUF_DEFINITIONS_LIB)
