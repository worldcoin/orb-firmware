project("Worldcoin firmware")

cmake_minimum_required(VERSION 3.19)

include(utils/cmake/utils.cmake)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

# Application
add_subdirectory(orb/app/boards/orb_v1)
add_subdirectory(orb/app/boards/stm32f3discovery)