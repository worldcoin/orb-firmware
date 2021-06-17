set(PROJECT_NAME "orbs_app")
project(${PROJECT_NAME})

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
endif ()

set(SDK_ROOT ../../../..)

set(SOURCES
        ${SDK_ROOT}/orbs/app/errors.c
        ${SDK_ROOT}/orbs/app/main.c
        ${SDK_ROOT}/orbs/app/freertos.c
        ${SDK_ROOT}/orbs/app/stm32f3xx_it.c
        ${SDK_ROOT}/orbs/app/stm32f3xx_hal_msp.c
        ${SDK_ROOT}/orbs/app/stm32f3xx_hal_timebase_tim.c
        ${SDK_ROOT}/orbs/app/system_stm32f3xx.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_gpio.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_dma.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_cortex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_exti.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2s.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2s_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim_ex.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tsc.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart.c
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart_ex.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/croutine.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/list.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/queue.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/timers.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
        )

set(INC_DIRS
        ${SDK_ROOT}/orbs/app/include
        ${SDK_ROOT}/orbs/app/boards/orbs_v1/config
        ${SDK_ROOT}/orbs/common/include
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Inc
        ${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Inc/Legacy
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/include
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
        ${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32F3xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        )

include_directories(${INC_DIRS})

add_definitions(-DDEBUG -DUSE_HAL_DRIVER -DSTM32F302xC)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32F302VBTX_FLASH.ld)

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
