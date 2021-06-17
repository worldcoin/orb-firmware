project("Worldcoin firmware")

cmake_minimum_required(VERSION 3.19)

include(utils/cmake/utils.cmake)

# Application
add_subdirectory(orbs/app/boards/orbs_v1)