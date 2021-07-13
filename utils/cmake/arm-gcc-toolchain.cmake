# This file must provide: full path to the compiler
# using variables:
# CMAKE_C_COMPILER
# CMAKE_CXX_COMPILER (CMake will guess that variable based on CMAKE_C_COMPILER)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Without that flag CMake is not able to pass test compilation check
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs")

# Get full path to compiler
execute_process(
        COMMAND which ${TOOLCHAIN_PREFIX}gcc
        OUTPUT_VARIABLE CMAKE_C_COMPILER
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get compiler directory
execute_process(
        COMMAND dirname ${CMAKE_C_COMPILER}
        OUTPUT_VARIABLE ARM_TOOLCHAIN_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

set(CMAKE_OBJCOPY ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_SIZE_UTIL ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}size CACHE INTERNAL "size tool")

set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
