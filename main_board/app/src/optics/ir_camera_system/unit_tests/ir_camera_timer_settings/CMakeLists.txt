# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

if ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(M64_MODE 1)
endif ()

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(tests_ir_camera_system)

set(ORB_DIR ${ZEPHYR_BASE}/../orb)
set(APP_DIR ${ORB_DIR}/main_board/app)

target_include_directories(testbinary PRIVATE
    mock_include
    ${APP_DIR}/src/optics/ir_camera_system
    ${ORB_DIR}/lib/include
    )
target_sources(testbinary PRIVATE
    ${APP_DIR}/src/optics/ir_camera_system/ir_camera_timer_settings.c
    main.c
    )
