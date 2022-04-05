# Proto2 Main MCU - Bootloader

## Getting started

First, follow the instructions in the [top-level README.md](../README.md).

## Compiling and Flashing

If you don't have development keys already created locally, then run
`./generate_dev_keys` while in the directory `"$REPO_DIR"/utils/ota/`.

Make sure you are in `"$REPO_DIR"/orb/bootloader_main/` directory.
Compile the bootloader:

```shell
# Passing the board is mandatory to build the bootloader
west build -b [mcu_main_v30 | mcu_main_v31 | mcu_sec]
```

- `mcu_main_v30`: Mainboard 3.0, used on Proto2 Orb
- `mcu_main_v31`: Mainboard 3.1, used on EVT Orb
- `mcu_sec`: security MCU, any version

Flash the bootloader, before the application:

If not in the Docker container:

```shell
west flash
```

If in the Docker container:

```shell
# Note: sometimes you must have the debugger already plugged into your computer _before_ launching Docker in order for Docker to see it.
su-exec root west flash
```