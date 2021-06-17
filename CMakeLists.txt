project("Worldcoin firmware")

cmake_minimum_required(VERSION 3.19)
set(CMAKE_TOOLCHAIN_FILE utils/arm-gcc-toolchain.cmake)

include(utils/utils.cmake)

# Application
add_subdirectory(orbs/app/boards/orbs_v1)