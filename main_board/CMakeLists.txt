cmake_minimum_required(VERSION 3.20.0)
set(TARGET app)

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
if (EXISTS ${PROJECT_DIR}/../public)
    get_filename_component(WORKSPACE_DIR ${PROJECT_DIR}/../../ ABSOLUTE)
else()
    get_filename_component(WORKSPACE_DIR ${PROJECT_DIR}/../ ABSOLUTE)
endif()

message(STATUS "Project directory: ${PROJECT_DIR}")
message(STATUS "Workspace directory: ${WORKSPACE_DIR}")

set(APP_FILES_COMPILE_OPTIONS -Wextra)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Building ${CMAKE_BUILD_TYPE}: optimized ⚡️")

    # Add configuration file to overwrite debug configuration
    list(APPEND OVERLAY_CONFIG "release.conf")
else ()
    message(STATUS "Building default: debug info included")
    add_compile_definitions(DEBUG)

    list(APPEND OVERLAY_CONFIG "debug.conf")
    set(DTC_OVERLAY_FILE
            "${DTC_OVERLAY_FILE} debug.overlay"
            CACHE STRING "" FORCE)
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

# optional Orb variable allows to select the board
if ("$ENV{ORB}" STREQUAL "pearl")
    unset(BOARD CACHE)
    set(ENV{BOARD} pearl_main)
elseif ("$ENV{ORB}" STREQUAL "diamond")
    unset(BOARD CACHE)
    set(ENV{BOARD} diamond_main)
endif ()

list(APPEND BOARD_ROOT ${PROJECT_DIR})
list(APPEND DTS_ROOT ${PROJECT_DIR})

# Load private & custom board configuration.
# Private config is not available publicly as the name suggests.
# The package is thus optional.
list(APPEND CMAKE_MODULE_PATH ${WORKSPACE_DIR}/orb/private/utils/cmake)
find_package(McuPrivate)

# Add a common dts overlay necessary to ensure the app is linked into,
# and fits inside, the slot0 partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
list(APPEND DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)

# In case we don't use West to build, add our local modules to the list
list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-mcu-main-app)

# SINE_TABLE_LENGTH defined for values from 0 to pi/2 (0..1)
set(SINE_TABLE_LENGTH 150)
add_compile_definitions(SINE_TABLE_LENGTH=${SINE_TABLE_LENGTH})
set(SINE_LUT_SRC_FILE ${CMAKE_BINARY_DIR}/zephyr/sine_lut.c)
add_custom_command(OUTPUT ${SINE_LUT_SRC_FILE}
    COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_DIR}/utils/math/generate_half_sine.py ${SINE_TABLE_LENGTH} > ${SINE_LUT_SRC_FILE})

set(SOURCES_FILES)
# common source files
list(APPEND SOURCES_FILES
    src/main.c

    src/optics/optics.c
    src/optics/1d_tof/tof_1d.c
    src/optics/ir_camera_system/ir_camera_timer_settings.c
    src/optics/ir_camera_system/ir_camera_system_hw.c
    src/optics/ir_camera_system/ir_camera_system.c
    src/optics/ir_camera_system/ir_leds_checks.c
    src/optics/liquid_lens/liquid_lens.c
    src/optics/mirror/mirror.c
    src/optics/mirror/mirror_private.c

    src/power/boot/boot.c

    src/runner/runner.c

    src/system/diag.c
    src/system/logs.c
    src/system/version/version.c
    src/system/stm32_timer_utils/stm32_timer_utils.c

    src/temperature/sensors/temperature.c
    src/temperature/fan/fan.c
    src/temperature/fan/fan_tests.c

    src/voltage_measurement/voltage_measurement.c

    ${SINE_LUT_SRC_FILE}
    src/ui/ui.c
    src/ui/ambient_light/als.c
    src/ui/button/button.c
    src/ui/rgb_leds/rgb_leds.c
    src/ui/rgb_leds/front_leds/front_leds.c
    src/ui/rgb_leds/operator_leds/operator_leds.c
    src/ui/sound/sound.c
    )

if (CONFIG_TRACING_USER)
    list(APPEND SOURCES_FILES src/system/tracing_user.c)
endif()

if (CONFIG_MEMFAULT_METRICS)
    list(APPEND SOURCES_FILES src/system/metrics.c)
endif()

if (CONFIG_BOARD_PEARL_MAIN)
    list(APPEND SOURCES_FILES src/gnss/gnss.c)
    list(APPEND SOURCES_FILES src/optics/mirror/homing_pearl.c)
    list(APPEND SOURCES_FILES src/power/battery/battery_ruby.c)
    list(APPEND SOURCES_FILES src/temperature/fan/fan_tach.c)
endif()

if (CONFIG_BOARD_DIAMOND_MAIN)
    list(APPEND SOURCES_FILES src/optics/mirror/homing_diamond.c)
    list(APPEND SOURCES_FILES src/power/battery/battery_amber.c)
    list(APPEND SOURCES_FILES src/optics/polarizer_wheel/drv8434s/drv8434s_private.c)
    list(APPEND SOURCES_FILES src/optics/polarizer_wheel/drv8434s/drv8434s.c)
    list(APPEND SOURCES_FILES src/optics/polarizer_wheel/polarizer_wheel.c)
    if (CONFIG_DT_HAS_DIAMOND_CONE_ENABLED)
        list(APPEND SOURCES_FILES src/ui/rgb_leds/cone_leds/cone_leds.c)
    endif()
    list(APPEND SOURCES_FILES src/ui/white_leds/white_leds.c)
    list(APPEND SOURCES_FILES src/temperature/fan/fan_tach_coarse.c)
endif()

if (CONFIG_ZTEST)
    add_compile_definitions(HIL_TEST=y)
    list(APPEND SOURCES_FILES
        src/voltage_measurement/voltage_measurement_tests.c
        src/optics/ir_camera_system/ir_camera_system_tests.c
        src/optics/mirror/mirror_tests.c
        src/system/dfu/dfu_tests.c
        src/temperature/fan/fan_tests.c
        src/ui/rgb_leds/front_leds/front_leds_tests.c
        src/ui/rgb_leds/operator_leds/operator_leds_tests.c
        src/pubsub/pubsub_tests.c
        )
    if (CONFIG_BOARD_DIAMOND_MAIN)
        list(APPEND SOURCES_FILES src/ui/rgb_leds/cone_leds/cone_leds_tests.c)
    endif()
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
zephyr_include_directories(config)

zephyr_linker_sources(SECTIONS
        config/compact-logs.ld
        config/orb-states.ld)

# link with Zephyr modules below
# - bootloader/mcuboot/boot/bootutil: public functions to access/modify mcuboot data from the application
if (CONFIG_MCUBOOT_BOOTUTIL_LIB)
    # check path is absolute
    if (IS_ABSOLUTE ${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE})
        set(MCUBOOT_SIGNATURE_KEY_FILE ${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE})
    else ()
        set(MCUBOOT_SIGNATURE_KEY_FILE ${WEST_TOPDIR}/${CONFIG_MCUBOOT_SIGNATURE_KEY_FILE})
    endif ()
    if (IS_ABSOLUTE ${CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE})
        set(MCUBOOT_ENCRYPTION_KEY_FILE ${CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE})
    else ()
        set(MCUBOOT_ENCRYPTION_KEY_FILE ${WEST_TOPDIR}/${CONFIG_MCUBOOT_ENCRYPTION_KEY_FILE})
    endif ()
    # check if files exist
    if (NOT EXISTS "${MCUBOOT_SIGNATURE_KEY_FILE}" OR NOT EXISTS "${MCUBOOT_ENCRYPTION_KEY_FILE}")
        message(FATAL_ERROR "Missing signature files, use 'generate_dev_keys'")
    else ()
        message(STATUS "Using signature key file: ${MCUBOOT_SIGNATURE_KEY_FILE}")
        message(STATUS "Using encryption key file: ${MCUBOOT_ENCRYPTION_KEY_FILE}")
    endif ()
    zephyr_link_libraries(MCUBOOT_BOOTUTIL)
endif ()
