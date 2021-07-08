
cmake_minimum_required(VERSION 3.19)
project("Worldcoin firmware")

include(utils/cmake/utils.cmake)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

# Application, when compiler is arm-none-eabi-gcc
if(CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc$")
    include(${CMAKE_TOOLCHAIN_FILE})

    message(STATUS "Building for ARM Cortex")
    message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
    add_subdirectory(orb/app/boards/orb_v1)
    add_subdirectory(orb/app/boards/stm32f3discovery)
endif()

# Tests
# run `cmake -D UNIT_TESTING=1`
if (UNIT_TESTING)
    message(STATUS "Importing Unit Testing libs")
    add_subdirectory(external/googletest)
    add_subdirectory(tests)
endif()
