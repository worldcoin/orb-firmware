# Main MCU application

## Getting started

### Download the source

First, follow the instructions in the [top-level README.md](../../README.md).

Once downloaded, `west` will check out this repository in the `orb` directory with a branch called `manifest-rev`. If
you want to work on the repo, make sure to check out the `main` branch and branch from there.

```shell
cd "$REPO_DIR"/orb
git remote add origin <repo-url.git>
git fetch
git checkout main

# From there, create a new branch
```

### Compiling and Flashing

Let's build and run the application, you have several options:

- the Makefile with Docker
- manually with west
- manually with CMake (from CLion for example)

> 💡 Important notes:
>
> - Firmware images are signed and encrypted. If you don't have development keys already created locally, then run
>   `./generate_dev_keys` while in the directory `"$REPO_DIR"/orb/public/utils/ota/`.
> - Make sure to have the [bootloader built and flashed](../../bootloader/README.md) with the keys _before_ flashing the application.
> - If you want to use the one-slot configuration (`-DDTC_OVERLAY_FILE=one_slot.overlay`) then the bootloader must have
>   been built with this option as well.

#### With Makefile

- Go to `${REPO_DIR}/orb/public/utils/docker`.
- Run `make help` to see all options

To Build: `make main_board-build`

To Flash: `make mcu-flash`

#### Manually

Make sure you are in `"$REPO_DIR"/orb/public/main_board` directory. Compile the app:

```shell
# 'west build' defaults to pearl_main and Debug build
# overlays might be applied to change any device or configuration
# below the partition table with `one_slot.overlay` and the test configuration with `tests.conf`
# can be combined
west build [-b pearl_main | diamond_main] [-- -DCMAKE_BUILD_TYPE=Release -DDTC_OVERLAY_FILE="one_slot.overlay;debug_uart_dts.overlay" -DOVERLAY_CONFIG="tests.conf"]
```

Flash the app:

If not in the Docker container:

```shell
west flash
```

If in the Docker container:

```shell
# Note: sometimes you must have the debugger already plugged into your computer _before_ launching Docker in order for Docker to see it.
su-exec root west flash
```

To flash a pre-built bootloader + app hex file:

```
su-exec root pyocd flash combined_pearl_main_<board_version>_<version>.hex --target stm32g474vetx -e chip
```

## Misc Documentation

### Developer options

Use `west build -DCONFIG_<option>=y` to use any of these convenience build options:

- `INSTA_BOOT`: do not wait for button press to proceed to boot
- `NO_JETSON_BOOT`: do not power up the Jetson
- `NO_SUPER_CAPS`: do not charge the super capacitors
- `MCU_DEVEL`: select all 4 options above

### Tests

#### Integration / HIL Tests

Use `west build -p -- -DOVERLAY_CONFIG="tests.conf"` to build the test firmware. This configuration uses
ZTest to perform tests from the running application. The configuration doesn't wait for someone to press the button
and doesn't boot the Jetson (`INSTA_BOOT` & `NO_JETSON_BOOT`, see above).

Twister can be used to compile and flash test configurations defined in `testcase.yaml`, here is an example,
with `pyocd` runner:

```shell
# From the `main_board` directory
# Load path to twister
source ../../../zephyr/zephyr-env.sh
# Run the test suites available in the current directory for board `pearl_main`:
twister -vv --testsuite-root . --board-root ../../boards/ --platform pearl_main \
 --clobber-output --device-serial=/dev/ttyXXX --device-testing --west-flash
```

- `--device-serial` is used to provide the path to the serial-to-USB dongle
- In case several debuggers are connected, append `--west-flash="-i=<UNIQUE_ID>"`. `UNIQUE_ID` is obtained
  by using `pyocd list`.
- When using a J-Link instead of an ST-Link add: `--west-runner=jlink`

Twister can be used to compile and flash test configurations defined in `testcase.yaml`, here is an example,
with `pyocd` runner:

```shell
twister -vv -T . -A ./../../boards/ -p pearl_main -c --test orb/public/main_board/orb.hil \
--device-serial /dev/ttyXXX --device-testing --west-flash="-i=<UNIQUE_ID>"
```

- `--device-serial` is used to provide the path to the serial to USB dongle
- `UNIQUE_ID` can be obtained by using `pyocd list` and passed to pyocd with the `-i` option

#### Unit tests

```shell
# From the `main_board` directory
# Load path to twister
source ../../../zephyr/zephyr-env.sh
# Run the test suites available in the current directory for board `pearl_main`:
twister -T . -vv -c -p unit_testing
```
