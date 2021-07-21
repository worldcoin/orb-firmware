
cmake_minimum_required(VERSION 3.19)
project("Orb firmware")

include(utils/cmake/utils.cmake)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

# Firmware compiled with arm-none-eabi-gcc
if(CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc$")
    include(${CMAKE_TOOLCHAIN_FILE})
    message(STATUS "Building for ARM microcontroller")

    add_subdirectory(orb/app/boards/orb_v1)
    add_subdirectory(orb/app/boards/stm32f3discovery)
endif()

# Tests compiled with host compiler
# run `cmake -D UNIT_TESTING=1`
if (UNIT_TESTING)
    message(STATUS "Building for Unit Tests")

    add_subdirectory(external/googletest)
    add_subdirectory(tests)
endif()
