cmake_minimum_required(VERSION 3.20.0)
set(TARGET app)

# Options
option(TEST_IR_CAMERA_SYSTEM "Run the IR camera system tests instead of the normal application on boot" OFF)
if (TEST_IR_CAMERA_SYSTEM)
    add_compile_definitions(CONFIG_TEST_IR_CAMERA_SYSTEM)
endif()

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ ABSOLUTE)
get_filename_component(WORKSPACE_DIR ${PROJECT_DIR} DIRECTORY)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")

    # Add configuration file to overwrite debug configuration
    list(APPEND OVERLAY_CONFIG "release.conf")
else ()
    message(STATUS "Minimal optimization, debug info included, logging enabled")
    #-fsanitize=undefined -fsanitize-undefined-trap-on-error
    add_compile_options(-Og -g)
    add_compile_definitions(DEBUG)

    list(APPEND OVERLAY_CONFIG "debug.conf")
endif ()

# Generate version for the current project
# CMake will re-run if file "VERSION" changes
include(${PROJECT_DIR}/utils/cmake/version.cmake)
application_version(APP_VERSION)
message(STATUS "Application version: ${APP_VERSION}")
set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
)

set_property(GLOBAL PROPERTY CSTD gnu11)

# default board is orb
if (DEFINED ENV{BOARD})
    set(BOARD $ENV{BOARD})
else ()
    set(BOARD mcu_main_v31)
endif ()

# Get our custom boards definitions before loading Zephyr
list(APPEND BOARD_ROOT ${PROJECT_DIR})
list(APPEND DTS_ROOT ${PROJECT_DIR})

# Add a common dts overlay necessary to ensure the app is linked into,
# and fits inside, the slot0 partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if (DTC_OVERLAY_FILE)
    set(DTC_OVERLAY_FILE
        "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay"
        CACHE STRING "" FORCE
        )
else()
    set(DTC_OVERLAY_FILE dts.overlay)
endif ()

# In case we don't use West to build, add our local modules to the list
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-mcu-main-app)

set(SOURCES_FILES
    src/main.c

    src/dfu/tests.c

    src/messaging/incoming_message_handling.c
    src/messaging/messaging_tests.c

    src/power_sequence/power_sequence.c
    src/fan/fan.c
    src/sound/sound.c

    src/button/button.c

    src/distributor_leds/distributor_leds.c
    src/distributor_leds/distributor_leds_tests.c

    src/front_unit_rgb_leds/front_unit_rgb_leds.c
    src/front_unit_rgb_leds/front_unit_led_patterns.c

    src/ir_camera_system/ir_camera_timer_settings.c
    src/ir_camera_system/ir_camera_system_test.c

    src/stepper_motors/stepper_motors.c
    src/stepper_motors/motors_tests.c

    src/temperature/temperature.c

    src/liquid_lens/liquid_lens.c

    src/version/version.c
    )

target_sources_ifdef(CONFIG_BOARD_MCU_MAIN_V30 ${TARGET} PRIVATE
    src/ir_camera_system/ir_camera_system_v30.c
    )

target_sources_ifdef(CONFIG_BOARD_MCU_MAIN_V31 ${TARGET} PRIVATE
    src/ir_camera_system/ir_camera_system_v31.c
    )

set(INCLUDE_DIRS
    include
    src
    )

target_sources(${TARGET} PRIVATE ${SOURCES_FILES})
target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})

# link with Zephyr modules below
# - modules/orb-protobuf-definitions: data format definitions and serializer/deserializer for messages between MCU, Jetson and Security MCU
# - bootloader/mcuboot/boot/bootutil: public functions to access/modify mcuboot data from the application
if (CONFIG_ORB_PROTOBUF_DEFINITIONS_LIB)
    zephyr_link_libraries(ORB_PROTOBUF_DEFINITIONS_LIB)
    # dependency below doesn't have to be forced if using West but must be added when using CMake only
    # to build zephyr_final
    add_dependencies(zephyr_final generate_proto_files)
endif ()
zephyr_link_libraries_ifdef(CONFIG_MCUBOOT_BOOTUTIL_LIB MCUBOOT_BOOTUTIL)

# specific target to test firmware update
# this target loads the new image in secondary slot and resets the target
# the bootloader will then swap the images before launching the new image as a Test
set(SIGNED_ENCRYPTED_BINARY ${CMAKE_BINARY_DIR}/zephyr/zephyr.signed.encrypted.bin)
add_custom_target(
        openocd_load_encrypted_sec_slot
        DEPENDS zephyr_final
        COMMAND openocd -f ${WORKSPACE_DIR}/orb/boards/arm/mcu_main/support/openocd.cfg -c "tcl_port disabled" -c "gdb_port disabled" -c "tcl_port disabled" -c "program \"${SIGNED_ENCRYPTED_BINARY}\" 0x8044000" -c reset -c shutdown
        # TODO flash "image OK" flags at the end of the secondary slot to mark image as pending
        COMMENT "Loading signed+encrypted binary into secondary slot"
)
