set(PROJECT_NAME secure_boot)

project(${PROJECT_NAME} C ASM)

set(SDK_ROOT ${CMAKE_SOURCE_DIR})
set(HAL_DIRECTORY STM32G4xx_HAL_Driver)

set(SRC_FILES
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_i2c_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_iwdg.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c

        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_interface_application.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_interface_bootloader.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_interface_common.c

        ../../sbsfu/Target/sfu_low_level.c
        ../../sbsfu/Target/sfu_low_level_flash.c
        ../../sbsfu/Target/sfu_low_level_flash_ext.c
        ../../sbsfu/Target/sfu_low_level_flash_int.c
        ../../sbsfu/Target/sfu_low_level_security.c
        ../../sbsfu/App/sfu_boot.c
        ../../sbsfu/App/sfu_com_loader.c
        ../../sbsfu/App/sfu_com_trace.c
        ../../sbsfu/App/sfu_error.c
        ../../sbsfu/App/sfu_fwimg_common.c
        ../../sbsfu/App/sfu_fwimg_no_swap.c
        ../../sbsfu/App/sfu_fwimg_swap.c
        ../../sbsfu/App/sfu_loader.c
        ../../sbsfu/App/sfu_mpu_isolation.c
        ../../sbsfu/App/sfu_new_image.c
        ../../sbsfu/App/sfu_test.c

        platform/stm32g4xx_hal_msp.c
        platform/stm32g4xx_it.c
        platform/SE_CORE_Bin.c
        platform/sfu_se_mpu.s
        platform/startup_stm32g474xx.s
        platform/syscalls.c
        platform/system_stm32g4xx.c

        ../../main.c
        )

set(INC_DIRS
        ../../include
        config
        ../../../se_core/include
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32G4xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Inc
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Interface
        ${SDK_ROOT}/components/Middlewares/STM32_Cryptographic/Fw_Crypto/STM32G4/Inc
        ${SDK_ROOT}/orb/secure_boot/sbsfu/App
        ${SDK_ROOT}/orb/secure_boot/sbsfu/Target
        ${SDK_ROOT}/orb/common/
        )

set_source_files_properties(platform/sfu_se_mpu.s platform/startup_stm32g474xx.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

include_directories(${INC_DIRS})

add_definitions(-DUSE_HAL_DRIVER -DSTM32G474xx)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32G474RETx.ld)

add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING;STM32G4_DISCOVERY)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-std=gnu11 -g3)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_compile_options(-Os -Wall -Wno-strict-aliasing -fstack-usage -MMD -MP)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(--specs=nano.specs)
add_link_options(-T ${LINKER_SCRIPT})

link_directories(${SDK_ROOT}/orb/common/)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${LINKER_SCRIPT})
add_dependencies(${PROJECT_NAME} se_core)

# generate binary
generate_object(${PROJECT_NAME} .bin binary)
# print executable size
firmware_size(${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -j .SE_IF_Code ${PROJECT_NAME} se_inter.elf > /dev/null 2>>1
        COMMAND ${CMAKE_OBJCOPY} --extract-symbol se_inter.elf se_interface_app.elf
        COMMAND ${CMAKE_OBJCOPY} -S --keep-symbols=${CMAKE_CURRENT_SOURCE_DIR}/se_interface.txt se_interface_app.elf se_interface_app.o
        COMMENT "Generating object file for SE interface to application")
