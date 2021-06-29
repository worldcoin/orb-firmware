set(PROJECT_NAME "orb_app_discovery")

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)

    # remove debug logs
    add_compile_definitions(CONFIG_LOG_LEVEL=0)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)

    # remove debug logs
    add_compile_definitions(CONFIG_LOG_LEVEL=0)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)

    # remove debug logs
    add_compile_definitions(CONFIG_LOG_LEVEL=0)
else ()
    message(STATUS "Minimal optimization, debug info included, logging enabled")
    add_compile_options(-Og -g)
    add_compile_definitions(DEBUG;CONFIG_LOG_LEVEL=4;USE_FULL_ASSERT)
endif ()

set(SDK_ROOT ../../../..)
set(HAL_DIRECTORY STM32F3xx_HAL_Driver)
set(MCU_LINE stm32f3xx)
set(FREERTOS_DIR ${SDK_ROOT}/external/freertos)
set(ORB_SOURCES
        ${SDK_ROOT}/orb/app/com.c
        ${SDK_ROOT}/orb/app/data_provider.c
        ${SDK_ROOT}/orb/app/freertos.c
        ${SDK_ROOT}/orb/app/main.c
        ${SDK_ROOT}/orb/app/logs.c
        ${SDK_ROOT}/orb/app/serializer.c
        ${SDK_ROOT}/orb/common/errors.c
        ./platform/${MCU_LINE}_it.c
        ./platform/${MCU_LINE}_hal_msp.c
        ./platform/${MCU_LINE}_hal_timebase_tim.c
        ./platform/system_${MCU_LINE}.c
        ./platform/startup_stm32f303xc.s
        )

set(SOURCES
        ${ORB_SOURCES}
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_adc.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_adc_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_crc.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_crc_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_rcc.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_rcc_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_gpio.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_dma.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_cortex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_pwr.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_pwr_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_flash.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2c.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2c_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_exti.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2s.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2s_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_spi.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_tim.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_tim_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_tsc.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_uart.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_uart_ex.c
        ${FREERTOS_DIR}/croutine.c
        ${FREERTOS_DIR}/event_groups.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/stream_buffer.c
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
        ${SDK_ROOT}/external/microcontroller-messaging-proto/c/mcu_messaging.pb.c
        ${SDK_ROOT}/external/nanopb/pb_common.c
        ${SDK_ROOT}/external/nanopb/pb_decode.c
        ${SDK_ROOT}/external/nanopb/pb_encode.c
        )

set_source_files_properties(./platform/startup_stm32f303xc.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

set_source_files_properties(${ORB_SOURCES} PROPERTIES COMPILE_FLAGS "-Wall -Werror -Wstrict-prototypes -Wwrite-strings -Wshadow -Winit-self -Wformat=2 -Wmissing-prototypes -Wstrict-overflow=2 -Wundef -Wconversion -Wno-missing-braces")

set(INC_DIRS
        ${SDK_ROOT}/orb/app/include
        ./config
        ${SDK_ROOT}/orb/common/include
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Inc
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Inc/Legacy
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32F3xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/external/microcontroller-messaging-proto/c
        ${SDK_ROOT}/external/nanopb
        )

project(${PROJECT_NAME} C ASM)

include_directories(${INC_DIRS})

add_definitions(-DUSE_HAL_DRIVER -DSTM32F303xC)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F303VCTx_FLASH.ld)

# hardware floating point
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})


add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE}
Building ${BIN_FILE}")
