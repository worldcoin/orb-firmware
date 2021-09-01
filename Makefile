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

stm32g4discovery: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	cmake --build build --target orb_app_$@.elf

stm32f3discovery: build/
	cmake $(CMAKE_ARM_GCC) . -B build
	cmake --build build --target orb_app_$@.elf

# you can use `flash` along with another target from above
# for example:
#   make flash stm32g4discovery
flash: $(filter-out flash,$(MAKECMDGOALS))
	openocd -f utils/debug/$<.cfg -c "tcl_port disabled" -c "gdb_port disabled" -c "tcl_port disabled" -c "program \"build/orb/app/boards/$</orb_app_$<.elf\"" -c reset -c shutdown

# pass the port to listen to
# for example:
# 	make logs /dev/tty.usbmodem22203
logs: $(filter-out logs,$(MAKECMDGOALS))
	python utils/debug/uart_dump.py -p $< -b 115200

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
