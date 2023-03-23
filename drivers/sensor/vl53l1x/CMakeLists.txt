# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(
	vl53l1x.c
    platform/src/vl53l1_platform.c
)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/platform/inc)
