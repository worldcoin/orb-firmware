# This Makefile make it easy to launch several builds using Make
# - Compile application firmware
# - Run Unit Tests
# - All in one

CC := -D CMAKE_C_COMPILER=arm-none-eabi-gcc
CXX := -D CMAKE_CXX_COMPILER=arm-none-eabi-c++

CMAKE_ARM_GCC := $(CC) $(CXX) -D CMAKE_TOOLCHAIN_FILE=./utils/cmake/arm-gcc-toolchain.cmake
CMAKE_DEBUG := -D CMAKE_BUILD_TYPE=Debug

CMAKE_COVERAGE_FLAGS := -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping"

# create directory if not present
build/:
	mkdir build

orb_app_discovery: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	make -C build $@.elf

orb_app: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	make -C build $@.elf

# run unit tests
unit_tests: clean build/
	cmake -D UNIT_TESTING=1 . -B build
	make -C build
	./build/tests/template_test/template_test --gtest_filter=* --gtest_color=yes

all: orb_app orb_app_discovery unit_tests

clean:
	rm -rf build
