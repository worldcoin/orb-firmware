# This directory is considered a Zephyr module for the build system
# to load our `drivers` and `lib` sub-directories
#
# This CMake file is loaded by West as a Zephyr module (`west list` must return the current directory)
# or, when using CMake without West, must be added to Zephyr modules manually (variable ZEPHYR_EXTRA_MODULES)
# before calling:
#    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

add_subdirectory(drivers)
add_subdirectory(lib)
