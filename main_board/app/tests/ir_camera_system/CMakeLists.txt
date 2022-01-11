# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

set(ENV{BOARD} native_posix_64)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(tests_ir_camera_system)
set(SOURCES
        ../../src/ir_camera_system/ir_camera_timer_settings.c
        main.c
        )
target_include_directories(app PRIVATE ../../src/ir_camera_system/)
target_sources(app PRIVATE ${SOURCES})
