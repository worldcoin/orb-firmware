# This file must provide: full path to the compiler by defining variables:
# - CMAKE_C_COMPILER
# - CMAKE_CXX_COMPILER
#
# The script can be called with `-DARM_TOOLCHAIN_DIR=path/to/compiler/bin`
# to force the usage of a compiler from within the directory

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX arm-zephyr-eabi-)

# Without that flag CMake is not able to pass test compilation check
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs")

# if CMAKE_C_COMPILER is not already defined
# & if path to C compiler ARM_TOOLCHAIN_DIR is not given
# we get the compiler by asking the environment path
if (NOT ARM_TOOLCHAIN_DIR AND NOT CMAKE_C_COMPILER)
    # Get full path to compiler from current environment
    execute_process(
            COMMAND which ${TOOLCHAIN_PREFIX}gcc
            OUTPUT_VARIABLE CMAKE_C_COMPILER
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT CMAKE_C_COMPILER)
        message(STATUS "Trying to find arm-none-eabi")

        set(TOOLCHAIN_PREFIX arm-none-eabi-)
        execute_process(
                COMMAND which ${TOOLCHAIN_PREFIX}gcc
                OUTPUT_VARIABLE CMAKE_C_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()

    if (NOT CMAKE_C_COMPILER)
        message(SEND_ERROR "Unable to find C compiler")
    endif ()
endif ()

if (NOT ARM_TOOLCHAIN_DIR AND CMAKE_C_COMPILER)
    # Get compiler directory
    execute_process(
            COMMAND dirname ${CMAKE_C_COMPILER}
            OUTPUT_VARIABLE ARM_TOOLCHAIN_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

if (ARM_TOOLCHAIN_DIR AND NOT CMAKE_C_COMPILER)
    # Use ARM_TOOLCHAIN_DIR to get the compiler path
    set(CMAKE_C_COMPILER ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}gcc)
endif ()

# Set C++ compiler
#set(CMAKE_CXX_COMPILER ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}g++)

if (NOT CMAKE_CXX_COMPILER)
    execute_process(
            COMMAND which ${TOOLCHAIN_PREFIX}g++
            OUTPUT_VARIABLE CMAKE_CXX_COMPILER
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT CMAKE_CXX_COMPILER)
        message(STATUS "Trying to find arm-none-eabi-g++")

        set(TOOLCHAIN_PREFIX arm-none-eabi-)
        execute_process(
                COMMAND which ${TOOLCHAIN_PREFIX}g++
                OUTPUT_VARIABLE CMAKE_CXX_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()
endif ()

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

if (NOT CMAKE_OBJCOPY)
    execute_process(
            COMMAND which ${TOOLCHAIN_PREFIX}objcopy
            OUTPUT_VARIABLE CMAKE_OBJCOPY
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT CMAKE_OBJCOPY)
        message(STATUS "Trying to find arm-none-eabi-objcopy")

        set(TOOLCHAIN_PREFIX arm-none-eabi-)
        execute_process(
                COMMAND which ${TOOLCHAIN_PREFIX}objcopy
                OUTPUT_VARIABLE CMAKE_OBJCOPY
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()
endif ()

if (NOT CMAKE_SIZE_UTIL)
    execute_process(
            COMMAND which ${TOOLCHAIN_PREFIX}size
            OUTPUT_VARIABLE CMAKE_SIZE_UTIL
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT CMAKE_OBJCOPY)
        message(STATUS "Trying to find arm-none-eabi-size")

        set(TOOLCHAIN_PREFIX arm-none-eabi-)
        execute_process(
                COMMAND which ${TOOLCHAIN_PREFIX}size
                OUTPUT_VARIABLE CMAKE_SIZE_UTIL
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif ()
endif ()

set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
