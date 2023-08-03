cmake_minimum_required(VERSION 3.20.0)
set(TARGET app)

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ ABSOLUTE)
get_filename_component(WORKSPACE_DIR ${PROJECT_DIR} DIRECTORY)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Building ${CMAKE_BUILD_TYPE}: optimized ⚡️")

    # Add configuration file to overwrite debug configuration
    list(APPEND OVERLAY_CONFIG "release.conf")
else ()
    message(STATUS "Building default: debug info included")
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
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_DIR}/VERSION
)

set_property(GLOBAL PROPERTY CSTD gnu11)

# default board is orb
if (DEFINED ENV{BOARD})
    set(BOARD $ENV{BOARD})
else ()
    set(BOARD mcu_main)
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
else ()
    set(DTC_OVERLAY_FILE dts.overlay)
endif ()

# In case we don't use West to build, add our local modules to the list
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-mcu-main-app)

# SINE_TABLE_LENGTH defined for values from 0 to pi/2 (0..1)
set(SINE_TABLE_LENGTH 150)
add_compile_definitions(SINE_TABLE_LENGTH=${SINE_TABLE_LENGTH})
set(SINE_LUT_SRC_FILE ${CMAKE_BINARY_DIR}/zephyr/sine_lut.c)
add_custom_command(OUTPUT ${SINE_LUT_SRC_FILE}
    COMMAND python3 ${PROJECT_DIR}/utils/math/generate_half_sine.py ${SINE_TABLE_LENGTH} > ${SINE_LUT_SRC_FILE})

set(SOURCES_FILES)
# common source files
list(APPEND SOURCES_FILES
    src/main.c

    src/gnss/gnss.c

    src/optics/optics.c
    src/optics/1d_tof/tof_1d.c
    src/optics/ir_camera_system/ir_camera_timer_settings.c
    src/optics/ir_camera_system/ir_camera_system_hw.c
    src/optics/ir_camera_system/ir_camera_system.c
    src/optics/liquid_lens/liquid_lens.c
    src/optics/mirrors/mirrors.c

    src/power/boot/boot.c
    src/power/battery/battery.c

    src/runner/runner.c

    src/system/logs.c
    src/system/version/version.c
    src/system/stm32_timer_utils/stm32_timer_utils.c

    src/temperature/sensors/temperature.c
    src/temperature/fan/fan.c
    src/temperature/fan/fan_tach.c

    ${SINE_LUT_SRC_FILE}
    src/ui/ambient_light/als.c
    src/ui/button/button.c
    src/ui/front_leds/front_leds.c
    src/ui/operator_leds/operator_leds.c
    src/ui/sound/sound.c
    )

if (CONFIG_ZTEST)
    add_compile_definitions(HIL_TEST=y)
    list(APPEND SOURCES_FILES
        src/optics/ir_camera_system/ir_camera_system_tests.c
        src/optics/mirrors/mirrors_tests.c
        src/system/dfu/dfu_tests.c
        src/temperature/fan/fan_tests.c
        src/ui/front_leds/front_leds_tests.c
        src/ui/operator_leds/operator_leds_tests.c
        src/pubsub/pubsub_tests.c
        )
else ()
    list(APPEND SOURCES_FILES
        src/pubsub/pubsub.c)
endif ()

set(INCLUDE_DIRS
    include
    src
    )

# EXTRA_COMPILE_FLAGS given via `west build -- -DEXTRA_COMPILE_FLAGS=...`
set_source_files_properties(${SOURCES_FILES} PROPERTIES COMPILE_OPTIONS "${APP_FILES_COMPILE_OPTIONS};${EXTRA_COMPILE_FLAGS}")

target_sources(${TARGET} PRIVATE ${SOURCES_FILES})
target_include_directories(${TARGET} PRIVATE ${INCLUDE_DIRS})

# link with Zephyr modules below
# - modules/orb-protobuf-definitions: data format definitions and serializer/deserializer for messages between MCU, Jetson and Security MCU
# - bootloader/mcuboot/boot/bootutil: public functions to access/modify mcuboot data from the application
if (CONFIG_ORB_PROTOBUF_DEFINITIONS_LIB)
    zephyr_link_libraries(ORB_PROTOBUF_DEFINITIONS_LIB)
    # dependency below doesn't have to be forced if using West but must be added when using CMake only
    # to build zephyr_final
    add_dependencies(app generate_proto_files)
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
