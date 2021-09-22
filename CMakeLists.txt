cmake_minimum_required(VERSION 3.13.1)

set(CMAKE_TOOLCHAIN_FILE utils/cmake/toolchain-gnuarmemb.cmake)
project("Orb firmware")

# Get custom boards definitions
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/orb)

# load environment variables or use defaults
if ($ENV{BOARD})
    set(BOARD $ENV{BOARD})
else()
    set(BOARD stm32g4_discovery)
endif()

if ($ENV{ZEPHYR_BASE})
    set(ZEPHYR_BASE $ENV{ZEPHYR_BASE})
else()
    set(ZEPHYR_BASE ${CMAKE_CURRENT_SOURCE_DIR}/zephyr)
endif()

# To add a custom module, append path to ZEPHYR_EXTRA_MODULES
# list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/my_custom_module)

# debug messages
message(STATUS "Zephyr modules: ${ZEPHYR_MODULES}")
message(STATUS "Zephyr extra modules: ${ZEPHYR_EXTRA_MODULES}")

# add our project
add_subdirectory(orb)
