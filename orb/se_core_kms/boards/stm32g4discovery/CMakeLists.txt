set(PROJECT_NAME se_core_kms)

project(${PROJECT_NAME} C ASM)

set(SDK_ROOT ${CMAKE_SOURCE_DIR})

set(SRC_FILES
        ../../data_init.c
        ../../ca_low_level.c
        ../../kms_low_level.c
        ../../nvms_low_level.c
        ../../se_low_level.c

        platform/startup_stm32g474xx.s
        platform/syscalls.c

        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/aes.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/asn1parse.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/asn1write.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/bignum.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/camellia.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ccm.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/cipher.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/cipher_wrap.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/cmac.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ctr_drbg.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ecdh.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ecdsa.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ecp.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/ecp_curves.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/entropy.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/entropy_poll.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/gcm.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/hmac_drbg.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/md.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/md_wrap.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/oid.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/pk.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/pk_wrap.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/pkparse.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/pkwrite.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/platform.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/platform_util.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/psa_crypto.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/psa_crypto_slot_management.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/rsa.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/rsa_internal.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/sha1.c
        ${SDK_ROOT}/components/Middlewares/Third_Party/mbed-crypto/library/sha256.c

        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_callgate.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_exception.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_fwimg.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_stack_smuggler_GNU.s
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_startup.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_user_application.c
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core/se_utils.c

        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Interface/CryptoApi/ca_core.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_der_x962.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_digest.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_dyn_obj.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_ecc.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_enc_dec.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_entry.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_init.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_key_mgt.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_mem.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_nvm_storage.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_objects.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core/kms_platf_objects.c
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules/kms_sign_verify.c

        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_crc_ex.c
#        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_firewall.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rng.c
        )

set(INC_DIRS
        config
        ../../include
        ${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32G4xx/Include
        ${SDK_ROOT}/components/Drivers/STM32G4xx_HAL_Driver/Inc
        ${SDK_ROOT}/components/Drivers/BSP/B-L4S5I-IOT01A
        ${SDK_ROOT}/components/Drivers/BSP/Components/Common
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Core
        ${SDK_ROOT}/components/Middlewares/STM32_Secure_Engine/Key
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Core
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Interface
        ${SDK_ROOT}/components/Middlewares/STM32_Key_Management_Services/Modules
        ${SDK_ROOT}/components/Drivers/CMSIS/Include
        ${SDK_ROOT}/components//Middlewares/Third_Party/mbed-crypto/include
        ${SDK_ROOT}/orb/secure_boot/sbsfu/App
        ${SDK_ROOT}/orb/secure_boot/sbsfu/Target
        ${SDK_ROOT}/orb/common/
        )

set_source_files_properties(platform/startup_stm32g474xx.s PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")

include_directories(${INC_DIRS})

##
## Prebuild
##

# if python doesn't find packages, make sure to have installed the environment
# PYTHONPATH can be set to force usage of specific packages

set(SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256 ON)
if (SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256)

    # create se_key.s file
    # TODO do not regenerate if dependencies didn't change

    set(KEY_FILE ${SDK_ROOT}/orb/se_core/se_key.s)
    file(WRITE ${KEY_FILE} "\t.section .SE_Key_Data,\"a\",%progbits\n\t.syntax unified\n\t.thumb\n")

    # TODO add Debug and Release targets in order to use a Debug and Release private key

    ## Prebuild target
    ## Base on prebuild.sh provided by STM, see orb/se_core/prebuild.sh.bak
    add_custom_target(generate_se_key_kms
            COMMAND python ${PREPARE_IMAGE_PY} conf ${SDK_ROOT}/orb/se_core_kms/include/se_crypto_config.h > crypto.txt

            COMMAND python ${PREPARE_IMAGE_PY} inject -k ${SDK_ROOT}/orb/common/ECCKEY.txt -f ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.pattern -p @ECDSA_BLOB_KEY@ ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp

            COMMAND python ${PREPARE_IMAGE_PY} inject -k ${SDK_ROOT}/orb/common/OEM_KEY_COMPANY1_key_AES_CBC.bin -f ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp -p @AES_BLOB_KEY@ ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h

            COMMAND cp ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp

            COMMAND python ${PREPARE_IMAGE_PY} inject -c ${SDK_ROOT}/orb/common/sbsfu_r0_crt.der -f ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp -p @SBSFU_ROOT_CA@ ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h

            COMMAND cp ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp

            COMMAND python ${PREPARE_IMAGE_PY} inject -c ${SDK_ROOT}/orb/common/sbsfu_inter1_oem1_crt.der -f ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp -p @SBSFU_OEM_CA_1@ ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h

            COMMAND rm ${SDK_ROOT}/orb/se_core_kms/include/kms_platf_objects_config.h.tmp

            COMMENT "Pre-build")

elseif (SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM OR SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256)

    message(WARNING "SECBOOT_AES128_GCM_AES128_GCM_AES128_GCM and SECBOOT_ECCDSA_WITHOUT_ENCRYPT_SHA256 crypto schemes are not implemented. You can look at STM's prebuild.sh script to implement them")

else ()

    message(WARNING "Please define crypto scheme")

endif ()

add_definitions(-DUSE_HAL_DRIVER -DSTM32G474xx -DMBEDTLS_CONFIG_FILE=<mbed_crypto_config.h> -DKMS_ENABLED -DENABLE_IMAGE_STATE_HANDLING)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32G474RETx.ld)

add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING;STM32G4_DISCOVERY)
add_compile_options(-mfloat-abi=soft -mfpu=fpv4-sp-d16)

add_compile_options(-mcpu=cortex-m4 -mthumb -mthumb-interwork)
add_compile_options(-std=gnu11 -g3)
add_compile_options(-ffunction-sections)
add_compile_options(-Os -Wall -Wno-strict-aliasing -fstack-usage -MMD -MP)
add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -mfpu=fpv4-sp-d16)
add_link_options(--specs=nano.specs)
add_link_options(-T ${LINKER_SCRIPT})

# Several directories to be included when linking
#   - Path to crypto lib
#   - ${SDK_ROOT}/orb/common/ points towards linker scripts specifying the addresses shared with SE, SBSFU and application
#     Those addresses will let SBDFU and application use the crypto functions
link_directories(${SDK_ROOT}/components/Middlewares/STM32_Cryptographic/Fw_Crypto/STM32G4/Lib ${SDK_ROOT}/orb/common/)
# Crypto lib linked in SE as well as libc (nano, see specs above) and libm)
link_libraries(c m libSTM32CryptographicV3.1.3_CM4_GCC.a)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${LINKER_SCRIPT})
add_dependencies(${PROJECT_NAME} generate_se_key_kms)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX .elf)

# generate binary
generate_object(${PROJECT_NAME} .bin binary)
# print executable size
firmware_size(${PROJECT_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin ${SDK_ROOT}/orb/common/${PROJECT_NAME}.bin
        COMMENT "Copying SE core binary to ${SDK_ROOT}/orb/secure_boot/${PROJECT_NAME}.bin")
