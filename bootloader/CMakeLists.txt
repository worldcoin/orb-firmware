cmake_minimum_required(VERSION 3.20.0)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)

    # Add configuration file to overwrite debug configuration
    set(OVERLAY_CONFIG "release.conf")
else ()
    message(STATUS "Minimal optimization, logging enabled")
    add_compile_definitions(DEBUG)
endif ()

# Add a common dts overlay necessary to ensure mcuboot is linked into,
# and fits inside, the boot partition. (If the user specified a
# DTC_OVERLAY_FILE on the CMake command line, we need to append onto
# the list).
if (DTC_OVERLAY_FILE)
    set(DTC_OVERLAY_FILE
            "${DTC_OVERLAY_FILE} ${CMAKE_CURRENT_LIST_DIR}/dts.overlay"
            CACHE STRING "" FORCE
            )
else ()
    set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/dts.overlay)
endif ()

# Enable Zephyr runner options which request mass erase if so
# configured.
#
# Note that this also disables the default "leave" option when
# targeting STM32 DfuSe devices with dfu-util, making the chip stay in
# the bootloader after flashing.
#
# That's the right thing, because mcuboot has nothing to do since the
# chip was just erased. The next thing the user is going to want to do
# is flash the application. (Developers can reset DfuSE devices
# manually to test mcuboot behavior on an otherwise erased flash
# device.)
macro(app_set_runner_args)
    if (CONFIG_ZEPHYR_TRY_MASS_ERASE)
        board_runner_args(dfu-util "--dfuse-modifiers=force:mass-erase")
        board_runner_args(pyocd "--flash-opt=-e=chip")
        board_runner_args(nrfjprog "--erase")
    endif ()
endmacro()

# Get our custom boards definitions before loading Zephyr
get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(WORKSPACE_DIR ${PROJECT_DIR}/../../ ABSOLUTE)

list(APPEND BOARD_ROOT ${PROJECT_DIR})
list(APPEND DTS_ROOT ${PROJECT_DIR})

set(SEC_BOARD_ROOT ${WORKSPACE_DIR}/orb/private)
if (EXISTS ${SEC_BOARD_ROOT})
    list(APPEND BOARD_ROOT ${SEC_BOARD_ROOT})
    list(APPEND DTS_ROOT ${SEC_BOARD_ROOT})
endif ()

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(orb-mcu-bootloader)

# Path to mcuboot
set(MCUBOOT_DIR $ENV{ZEPHYR_BASE}/../bootloader/mcuboot)
# Path to "boot" subdirectory of mcuboot repository.
set(BOOT_DIR ${MCUBOOT_DIR}/boot)

# Path to tinycrypt library source subdirectory of MCUBOOT_DIR.
set(TINYCRYPT_DIR "${MCUBOOT_DIR}/ext/tinycrypt/lib")
assert_exists(TINYCRYPT_DIR)
set(TINYCRYPT_SHA512_DIR "${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib")
assert_exists(TINYCRYPT_SHA512_DIR)
# Path to crypto-fiat
set(FIAT_DIR "${MCUBOOT_DIR}/ext/fiat")
assert_exists(FIAT_DIR)
# Path to mbed-tls' asn1 parser library.
set(MBEDTLS_ASN1_DIR "${MCUBOOT_DIR}/ext/mbedtls-asn1")
assert_exists(MBEDTLS_ASN1_DIR)

zephyr_library_include_directories(
        include
)

# Zephyr port-specific sources.
zephyr_library_sources(
        src/main.c
        src/flash_map_extended.c
        src/os.c
        src/keys.c
)

if (NOT DEFINED CONFIG_FLASH_PAGE_LAYOUT)
    zephyr_library_sources(
            src/flash_map_legacy.c
    )
endif ()

# Generic bootutil sources and includes.
zephyr_library_include_directories(${BOOT_DIR}/bootutil/include)
zephyr_library_sources(
        ${BOOT_DIR}/bootutil/src/image_validate.c
        ${BOOT_DIR}/bootutil/src/tlv.c
        ${BOOT_DIR}/bootutil/src/encrypted.c
        ${BOOT_DIR}/bootutil/src/image_rsa.c
        ${BOOT_DIR}/bootutil/src/image_ecdsa.c
        ${BOOT_DIR}/bootutil/src/image_ed25519.c
        ${BOOT_DIR}/bootutil/src/bootutil_misc.c
        ${BOOT_DIR}/bootutil/src/fault_injection_hardening.c
)

# library which might be common source code for MCUBoot and an application
zephyr_link_libraries(MCUBOOT_BOOTUTIL)

if (CONFIG_BOOT_FIH_PROFILE_HIGH)
    zephyr_library_sources(
            ${BOOT_DIR}/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
    )
endif ()

if (CONFIG_SINGLE_APPLICATION_SLOT)
    zephyr_library_sources(
            ${BOOT_DIR}/zephyr/single_loader.c
    )
    zephyr_library_include_directories(${BOOT_DIR}/bootutil/src)
else ()
    zephyr_library_sources(
            ${BOOT_DIR}/bootutil/src/loader.c
            ${BOOT_DIR}/bootutil/src/swap_misc.c
            ${BOOT_DIR}/bootutil/src/swap_scratch.c
            ${BOOT_DIR}/bootutil/src/swap_move.c
            ${BOOT_DIR}/bootutil/src/caps.c
    )
endif ()

if (CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256 OR CONFIG_BOOT_ENCRYPT_EC256)
    zephyr_library_include_directories(
            ${MBEDTLS_ASN1_DIR}/include
    )
    zephyr_library_sources(
            # Additionally pull in just the ASN.1 parser from mbedTLS.
            ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
            ${MBEDTLS_ASN1_DIR}/src/platform_util.c
    )
    if (CONFIG_BOOT_USE_TINYCRYPT)
        # When using ECDSA signatures, pull in our copy of the tinycrypt library.
        zephyr_library_include_directories(
                ${BOOT_DIR}/zephyr/include
                ${TINYCRYPT_DIR}/include
        )
        zephyr_include_directories(${TINYCRYPT_DIR}/include)

        zephyr_library_sources(
                ${TINYCRYPT_DIR}/source/ecc.c
                ${TINYCRYPT_DIR}/source/ecc_dsa.c
                ${TINYCRYPT_DIR}/source/sha256.c
                ${TINYCRYPT_DIR}/source/utils.c
        )
    endif ()

    # Since here we are not using Zephyr's mbedTLS but rather our own, we need
    # to set MBEDTLS_CONFIG_FILE ourselves. When using Zephyr's copy, this
    # variable is set by its Kconfig in the Zephyr codebase.
    zephyr_library_compile_definitions(
            MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/include/mcuboot-mbedtls-cfg.h"
    )
elseif (CONFIG_BOOT_SIGNATURE_TYPE_NONE)
    zephyr_library_include_directories(
            ${BOOT_DIR}/zephyr/include
            ${TINYCRYPT_DIR}/include
    )

    zephyr_library_sources(
            ${TINYCRYPT_DIR}/source/sha256.c
            ${TINYCRYPT_DIR}/source/utils.c
    )
elseif (CONFIG_BOOT_SIGNATURE_TYPE_RSA)
    # Use mbedTLS provided by Zephyr for RSA signatures. (Its config file
    # is set using Kconfig.)
    zephyr_include_directories(include)
elseif (CONFIG_BOOT_SIGNATURE_TYPE_ED25519 OR CONFIG_BOOT_ENCRYPT_X25519)
    if (CONFIG_BOOT_USE_TINYCRYPT)
        zephyr_library_include_directories(
                ${MBEDTLS_ASN1_DIR}/include
                ${BOOT_DIR}/zephyr/include
                ${TINYCRYPT_DIR}/include
                ${TINYCRYPT_SHA512_DIR}/include
        )
        zephyr_library_sources(
                ${TINYCRYPT_DIR}/source/sha256.c
                ${TINYCRYPT_DIR}/source/utils.c
                ${TINYCRYPT_SHA512_DIR}/source/sha512.c
                # Additionally pull in just the ASN.1 parser from mbedTLS.
                ${MBEDTLS_ASN1_DIR}/src/asn1parse.c
                ${MBEDTLS_ASN1_DIR}/src/platform_util.c
        )
        zephyr_library_compile_definitions(
                MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/include/mcuboot-mbedtls-cfg.h"
        )
    else ()
        zephyr_include_directories(include)
    endif ()

    zephyr_library_include_directories(
            ${BOOT_DIR}/zephyr/include
            ${FIAT_DIR}/include/
    )

    zephyr_library_sources(
            ${FIAT_DIR}/src/curve25519.c
    )
endif ()

if (CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_ENCRYPT_X25519)
    zephyr_library_sources(
            ${TINYCRYPT_DIR}/source/aes_encrypt.c
            ${TINYCRYPT_DIR}/source/aes_decrypt.c
            ${TINYCRYPT_DIR}/source/ctr_mode.c
            ${TINYCRYPT_DIR}/source/hmac.c
            ${TINYCRYPT_DIR}/source/ecc_dh.c
    )
endif ()

if (CONFIG_BOOT_ENCRYPT_EC256)
    zephyr_library_sources(
            ${TINYCRYPT_DIR}/source/ecc_dh.c
    )
endif ()

if (CONFIG_MCUBOOT_SERIAL)
    message(ERROR "Serial is not an option for us")
endif ()

if (NOT CONFIG_BOOT_SIGNATURE_KEY_FILE STREQUAL "")
    # CONF_FILE points to the KConfig configuration files of the bootloader.
    foreach (filepath ${CONF_FILE})
        file(READ ${filepath} temp_text)
        string(FIND "${temp_text}" ${CONFIG_BOOT_SIGNATURE_KEY_FILE} match)
        if (${match} GREATER_EQUAL 0)
            if (NOT DEFINED CONF_DIR)
                get_filename_component(CONF_DIR ${filepath} DIRECTORY)
            else ()
                message(FATAL_ERROR "Signature key file defined in multiple conf files")
            endif ()
        endif ()
    endforeach ()

    if (IS_ABSOLUTE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
        set(KEY_FILE ${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    elseif ((DEFINED CONF_DIR) AND
    (EXISTS ${CONF_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE}))
        set(KEY_FILE ${CONF_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    else ()
        set(KEY_FILE ${WORKSPACE_DIR}/${CONFIG_BOOT_SIGNATURE_KEY_FILE})
    endif ()
    message("MCUBoot bootloader signature key file: ${KEY_FILE}")

    set(GENERATED_PUBKEY ${ZEPHYR_BINARY_DIR}/autogen-pubkey.c)
    add_custom_command(
            OUTPUT ${GENERATED_PUBKEY}
            COMMAND
            ${PYTHON_EXECUTABLE}
            ${MCUBOOT_DIR}/scripts/imgtool.py
            getpub
            -k
            ${KEY_FILE}
            > ${GENERATED_PUBKEY}
            DEPENDS ${KEY_FILE}
    )
    zephyr_library_sources(${GENERATED_PUBKEY})
endif ()

# Create C file embedding the encryption private key
# based on PEM file containing the private key
# PEM file should NOT be tracked using Git
# Make sure to recreate the same PEM file based on the same private key
if (CONFIG_BOOT_ENCRYPT_EC256)
    # use environment variable if it exists
    if (DEFINED ENV{ENC_KEY_FILE})
        set(ENC_KEY_FILE $ENV{ENC_KEY_FILE})
    else ()
        set(ENC_KEY_FILE ${PROJECT_DIR}/utils/ota/enc-ec256-priv.pem)
    endif ()

    if (NOT EXISTS ${ENC_KEY_FILE})
        message(FATAL_ERROR "Cannot find private key file, you can generate local dev keys by running 'generate_dev_keys' script")
    else ()
        set(GENERATED_ENC_PRIVKEY ${CMAKE_CURRENT_SOURCE_DIR}/share/autogen-enc-privkey.c)
        add_custom_command(
                OUTPUT ${GENERATED_ENC_PRIVKEY}
                COMMAND imgtool getpriv -k "${ENC_KEY_FILE}" --minimal > ${GENERATED_ENC_PRIVKEY}
        )
        zephyr_library_sources(${GENERATED_ENC_PRIVKEY})
    endif ()
endif ()

if (CONFIG_MCUBOOT_CLEANUP_ARM_CORE)
    zephyr_library_sources(
            ${BOOT_DIR}/zephyr/arm_cleanup.c
    )
endif ()
