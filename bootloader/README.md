# Bootloader

## Getting started

First, follow the instructions in the [top-level README.md](../README.md).

## Compiling and Flashing

If you don't have development keys already created locally, then run
`./generate_dev_keys` while in the directory `"$REPO_DIR"/utils/ota/`.

Make sure you are in `"$REPO_DIR"/orb/bootloader/` directory.
Compile the bootloader for the main microcontroller:

```shell
# Passing the board is mandatory to build the bootloader
west build -b [pearl_main | diamond_main]  [-- -DDTC_OVERLAY_FILE=one_slot.overlay]
```

or for the security microcontroller (internal use only):

```shell
west build -b [pearl_security | diamond_security] -- -DBOARD_ROOT=../sec_board
```

Board options:

- `pearl_main`: main board v3.2+, used on Pearl Orbs
- `pearl_security`: security MCU, any version, used on Pearl Orbs
- `diamond_main`: main board, used on Diamond Orbs
- `diamond_security`: security MCU, used on Diamond Orbs

Custom options:

- `-DDTC_OVERLAY_FILE=one_slot.overlay`: use this if you want to flash a `pearl_main` application with the one-slot
  configuration because the image doesn't fit the default partitions. The one-slot configuration shouldn't be needed for
  any other board.
- `-DBOARD_ROOT=../sec_board`: needed to build the bootloader for the security microcontrollers as the board definitions
  are not included in the main repository but taken from the west project in `sec_board/` pulled when the `internal`
  group is included in the west configuration.

Flash the bootloader before the application:

```shell
west flash
```

From Docker:

```shell
# Note: sometimes you must have the debugger already plugged into your computer _before_ launching Docker in order for Docker to see it.
su-exec root west flash
```
