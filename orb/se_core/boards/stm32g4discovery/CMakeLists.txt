set(PROJECT_NAME se_core)

project(${PROJECT_NAME} C ASM)

set(SDK_ROOT ${CMAKE_SOURCE_DIR})
set(HAL_DIRECTORY STM32G4xx_HAL_Driver)

set(SRC_FILES
        ../../data_init.c
        ../../se_key.s
        ../../se_crypto_bootloader.c
        ../../se_low_level.c

        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_callgate.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_crypto_common.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_exception.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_fwimg.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_startup.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_user_application.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_utils.c
        )

set(INC_DIRS
        ../../include
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32G4xx/Include
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Inc
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Key
        ${SDK_ROOT}/components/Middlewares/STM32_Cryptographic/Fw_Crypto/STM32G4/Inc
        ${SDK_ROOT}/orb/secure_boot/sbsfu/App
        ${SDK_ROOT}/orb/secure_boot/sbsfu/Target
        ${SDK_ROOT}/orb/common/
        )

set_source_files_properties(../../se_key.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

include_directories(${INC_DIRS})

##
## Prebuild
##
set(PREPARE_IMAGE_PY ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Utilities/KeysAndImages/prepareimage.py)

# if python doesn't find packages, make sure to have installed the environment
# PYTHONPATH can be set to force usage of specific packages

if (SECBOOT_ECCDSA_WITH_AES128_CBC_SHA256)

    # create se_key.s file
    # TODO do not regenerate if dependencies didn't change

    set(KEY_FILE ${SDK_ROOT}/orb/se_core/se_key.s)
    file(WRITE ${KEY_FILE} "\t.section .SE_Key_Data,\"a\",%progbits\n\t.syntax unified\n\t.thumb\n")

    # TODO add Debug and Release targets in order to use a Debug and Release private key

    ## Prebuild target
    ## Base on prebuild.sh provided by STM, see orb/se_core/prebuild.sh.bak
    add_custom_target(generate_se_key
            COMMAND python ${PREPARE_IMAGE_PY} conf ${SDK_ROOT}/orb/se_core/include/se_crypto_config.h > crypto.txt
            COMMAND python ${PREPARE_IMAGE_PY} trans -a GNU -k ${SDK_ROOT}/orb/se_core/key_aes_cbc.bin -f SE_ReadKey_1 -v V7M >> ${KEY_FILE}
            COMMAND python ${PREPARE_IMAGE_PY} trans -a GNU -k ${SDK_ROOT}/orb/se_core/ECCKEY1.txt -f SE_ReadKey_1_Pub -v V7M >> ${KEY_FILE}
            COMMAND echo "  .end" >> ${KEY_FILE}
            COMMENT "Pre-build")

elseif (SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM OR SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256)

    message(WARNING "SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM and SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256 crypto schemes are not implemented. You can look at STM's prebuild.sh script to implement them")

else ()

    message(WARNING "Please define crypto scheme")

endif ()

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

# Several directories to be included when linking
#   - Path to crypto lib
#   - ${SDK_ROOT}/orb/common/ points towards linker scripts specifying the addresses shared with SE, SBSFU and application
#     Those addresses will let SBDFU and application use the crypto functions
link_directories(${SDK_ROOT}/components/Middlewares/STM32_Cryptographic/Fw_Crypto/STM32G4/Lib ${SDK_ROOT}/orb/common/)
# Crypto lib linked in SE as well as libc (nano, see specs above) and libm)
link_libraries(c m libSTM32CryptographicV3.1.3_CM4_GCC_fpu.a)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${LINKER_SCRIPT})
add_dependencies(${PROJECT_NAME} generate_se_key)

# generate binary
generate_object(${PROJECT_NAME} .bin binary)
# print executable size
firmware_size(${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin ${SDK_ROOT}/orb/secure_boot/${PROJECT_NAME}.bin
        COMMENT "Copying binary")

