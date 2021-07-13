# This Makefile make it easy to launch several builds using Make
# - Compile application firmware
# - Run Unit Tests
# - All in one

CMAKE_ARM_GCC := -DCMAKE_TOOLCHAIN_FILE=./utils/cmake/arm-gcc-toolchain.cmake
CMAKE_DEBUG := -DCMAKE_BUILD_TYPE=Debug

CMAKE_COVERAGE_FLAGS := -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping"

# create directory if not present
build/:
	mkdir build

orb_app_discovery: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	cmake --build build --target $@.elf

orb_app: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	cmake --build build --target $@.elf

apps: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	cmake --build build

# run unit tests
unit_tests: clean build/
	cmake -D UNIT_TESTING=1 . -B build
	cmake --build build
	./build/tests/template_test/template_test --gtest_filter=* --gtest_color=yes

all: apps unit_tests

clean:
	rm -rf build
