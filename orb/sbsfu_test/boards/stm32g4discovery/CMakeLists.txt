set(PROJECT_NAME "sbsfu_test")

set(SDK_ROOT ../../../..)
set(HAL_DIRECTORY STM32G4xx_HAL_Driver)
set(MCU_LINE stm32g4xx)

set(SRC_FILES
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_iwdg.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c

        ../../com.c
        ../../common.c
        ../../flash_if.c
        ../../fw_update_app.c
        ../../main.c
        ../../se_user_code.c
        ../../test_protections.c
        ../../ymodem.c
        ../../sfu_app_new_image.c

        platform/syscalls.c
        platform/startup_stm32g474xx.s
        platform/system_stm32g4xx.c
        platform/stm32g4xx_it.c

        ${CMAKE_SOURCE_DIR}/orb/common/se_interface_app.o
        )

set_source_files_properties(./platform/startup_stm32g474xx.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

set(INC_DIRS
        ../../include
        ${SDK_ROOT}/orb/common/
        ./config
        ${SDK_ROOT}/orb/common/include
        ${SDK_ROOT}/components/Drivers/${HAL_DIRECTORY}/Inc
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32G4xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/
        ${SDK_ROOT}/orb/se_core/include/
        ${SDK_ROOT}/orb/secure_boot/sbsfu/App/
        )

project(${PROJECT_NAME} C ASM)
include_directories(${INC_DIRS})
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32G474RETx.ld)


add_definitions(-DUSE_HAL_DRIVER -DSTM32G474xx)
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING;STM32G4_DISCOVERY)
# hardware floating point
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})
link_directories(${SDK_ROOT}/orb/common/)

#link_libraries()
add_executable(${PROJECT_NAME} ${SRC_FILES} ${LINKER_SCRIPT})
add_dependencies(${PROJECT_NAME} secure_boot)

# TODO Link se_interface_app.o