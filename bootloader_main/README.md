# Proto2 Main MCU - Bootloader

## Getting started

Go to [the workspace repository](https://github.com/worldcoin/proto2-firmware)
and follow the instructions in the README.md.

## Compiling and Flashing

Make sure you are in `"$REPO_DIR"/orb/bootloader_main/` directory.
Compile the bootloader:

```shell
# Passing the board is mandatory to build the bootloader
west build -b [mcu_main_v30 | mcu_main_v31]
```

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
