# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

project(tests_ir_camera_system)

if ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(M64_MODE 1)
endif ()

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

get_filename_component(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
get_filename_component(ORB_DIR "${APP_DIR}/.." ABSOLUTE)

add_compile_definitions(IR_CAMERA_UNIT_TESTS=1)

target_include_directories(testbinary PRIVATE
    mock_include
    ${APP_DIR}/src/optics/ir_camera_system
    ${APP_DIR}/src/
    ${ORB_DIR}/lib/include
    )

target_sources(testbinary PRIVATE
    ${APP_DIR}/src/optics/ir_camera_system/ir_camera_system.c
    main.c
    )

add_custom_command(
    OUTPUT main.c
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/jinja_expand.py ${CMAKE_CURRENT_SOURCE_DIR} template_main.c main.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/jinja_expand.py ${CMAKE_CURRENT_SOURCE_DIR}/template_main.c
)
