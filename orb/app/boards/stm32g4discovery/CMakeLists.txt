set(PROJECT_NAME "orb_app_stm32g4discovery")

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
    add_compile_options(-Og -g -fsanitize=undefined -fsanitize-undefined-trap-on-error)
    add_compile_definitions(DEBUG;CONFIG_LOG_LEVEL=4;USE_FULL_ASSERT;configCHECK_FOR_STACK_OVERFLOW=2)
endif ()

set(SDK_ROOT ../../../..)
set(HAL_DIRECTORY STM32G4xx_HAL_Driver)
set(MCU_LINE stm32g4xx)
set(FREERTOS_DIR ${SDK_ROOT}/external/freertos)
set(ORB_SOURCES
        ${SDK_ROOT}/orb/app/control.c
#        ${SDK_ROOT}/orb/app/com.c
        ${SDK_ROOT}/orb/app/data_provider.c
        ${SDK_ROOT}/orb/app/deserializer.c
        ${SDK_ROOT}/orb/app/freertos.c
#        ${SDK_ROOT}/orb/app/imu.c
        ${SDK_ROOT}/orb/app/main.c
        ${SDK_ROOT}/orb/app/logs.c
        ${SDK_ROOT}/orb/app/serializer.c
        ${SDK_ROOT}/orb/app/watchdog.c
        ${SDK_ROOT}/orb/common/diag.c
        ${SDK_ROOT}/orb/common/errors.c
        ${SDK_ROOT}/orb/common/hardfault.c
        ./platform/${MCU_LINE}_it.c
        ./platform/${MCU_LINE}_hal_msp.c
        ./platform/${MCU_LINE}_hal_timebase_tim.c
        ./platform/${MCU_LINE}_clocks.c
        ./platform/system_${MCU_LINE}.c
        ./platform/startup_stm32g474xx.s
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
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_exti.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_flash.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2c.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2c_ex.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_i2s.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_iwdg.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_spi.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_tim.c
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Src/${MCU_LINE}_hal_tim_ex.c
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

set_source_files_properties(./platform/startup_stm32g474xx.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

set_source_files_properties(${ORB_SOURCES} PROPERTIES COMPILE_FLAGS "-Wall -Werror -Wstrict-prototypes -Wwrite-strings -Wshadow -Winit-self -Wformat=2 -Wmissing-prototypes -Wstrict-overflow=2 -Wundef -Wconversion -Wno-missing-braces")

set(INC_DIRS
        ${SDK_ROOT}/orb/app/include
        ./config
        ${SDK_ROOT}/orb/common/include
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Inc
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Inc/Legacy
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32G4xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/external/microcontroller-messaging-proto/c
        ${SDK_ROOT}/external/nanopb
        )

project(${PROJECT_NAME} C ASM)

include_directories(${INC_DIRS})

add_definitions(-DUSE_HAL_DRIVER -DSTM32G474xx)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32G474RETx_FLASH.ld)

# hardware floating point
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING;STM32G4_DISCOVERY)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})

add_custom_command(OUTPUT config/version.h
        COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/${SDK_ROOT}/utils/version/gen_version.py -f ${CMAKE_CURRENT_SOURCE_DIR}/version.ini -i ${CMAKE_CURRENT_SOURCE_DIR}/config/version.h
        DEPENDS version.ini
        COMMENT "Generating version"
        )

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT} config/version.h)

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

firmware_size(${PROJECT_NAME}.elf)
generate_object(${PROJECT_NAME}.elf .hex ihex)
generate_object(${PROJECT_NAME}.elf .bin binary)
