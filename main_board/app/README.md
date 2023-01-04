# Main MCU application

## Getting started

### Download the source

First, follow the instructions in the [top-level README.md](../../README.md).

Once downloaded, `west` will check out this repository in the `orb` directory with a branch called `manifest-rev`. If you
want to work on the repo, make sure to check out the `main` branch and branch from there.

```shell
cd "$REPO_DIR"/orb
git remote add origin git@github.com:worldcoin/orb-mcu-firmware.git
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
>   `./generate_dev_keys` while in the directory `"$REPO_DIR"/orb/utils/ota/`.
> - Make sure to have the [bootloader flashed](../../bootloader_main/README.md) _before_ flashing the application.

#### With Makefile

- Go to `${REPO_DIR}/orb/utils/docker`.
- Run `make help` to see all options

To Build: `make main_board-build`

To Flash: `make mcu-flash`

#### Manually

Make sure you are in `"$REPO_DIR"/orb/main_board/app/` directory. Compile the app:

```shell
# 'west build' defaults to mcu_main
west build -b [stm32g484_eval | mcu_main]
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
su-exec root pyocd flash combined_mcu_main_<board_version>_<version>.hex --target stm32g474vetx -e chip
```

## Misc Documentation

### Cmake build options

#### Developer options

Use `west build -DCONFIG_<option>=y` to use any of these convenience build options:

- `INSTA_BOOT`
- `NO_JETSON_BOOT`
- `NO_BOOT_LEDS`
- `NO_SUPER_CAPS`
- `MCU_DEVEL`

#### Tests

Use `west build -p -- -DOVERLAY_CONFIG="tests.conf"` to build the test firmware. This configuration uses
ZTest to perform tests from the running application. The configuration doesn't wait for someone to press the button
and doesn't boot the Jetson (`INSTA_BOOT` & `NO_JETSON_BOOT`).

### Memory map

| Region                              | Location   | Size  |
| ----------------------------------- | ---------- | ----- |
| [Bootloader](../../bootloader_main) | 0x00000000 | 48kB  |
| Application Primary Slot            | 0x0000C000 | 224kB |
| Application Secondary Slot          | 0x00044000 | 224kB |
| Scratch partition                   | 0x0007C000 | 8kB   |
| Persistent storage                  | 0x0007E000 | 8kB   |

### Timer allocations - Mainboard 3.1

| Peripheral                     | Pin  | Timer | Timer Channel |
| ------------------------------ | ---- | ----- | ------------- |
| 740nm                          | PB0  | TIM3  | CH3           |
| 850nm Left                     | PB14 | TIM15 | CH1           |
| 850nm Right                    | PB15 | TIM15 | CH2           |
| 940nm Left                     | PE2  | TIM3  | CH1           |
| 940nm Right                    | PE5  | TIM3  | CH4           |
| Main fan PWM                   | PB2  | TIM5  | CH1           |
| Main fan tach                  | PD7  | TIM2  | CH3           |
| Aux fan PWM                    | PA1  | TIM5  | CH2           |
| Aux fan tach                   | PA12 | TIM4  | CH2           |
| CAM 0 (IR eye camera) trigger  | PC8  | TIM8  | CH3           |
| CAM 2 (IR face camera) trigger | PC9  | TIM8  | CH4           |
| CAM 3 (IR face camera) trigger | PC6  | TIM8  | CH1           |
| User LEDs                      | PE3  | TIM20 | CH2           |
