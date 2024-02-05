# Bootloader

## Getting started

First, follow the instructions in the [top-level README.md](../README.md).

## Compiling and Flashing

If you don't have development keys already created locally, then run
`./generate_dev_keys` while in the directory `"$REPO_DIR"/utils/ota/`.

Make sure you are in `"$REPO_DIR"/orb/bootloader/` directory.
Compile the bootloader:

```shell
# Passing the board is mandatory to build the bootloader
west build -b [pearl_main | pearl_security | diamond_main | diamond_security]  [-- -DDTC_OVERLAY_FILE=one_slot.overlay]
```

- `pearl_main`: main board v3.1+, used on Pearl Orbs
- `pearl_security`: security MCU, any version, used on Pearl Orbs
- `diamond_main`: Main MCU, used on Diamond Orb Mainboards v4.1+,
- `diamond_security`: Security MCU, used on Diamond Orb Mainboards v4.1+,

- `-DDTC_OVERLAY_FILE=one_slot.overlay`: Use this if you want to flash a Main MCU application with the one-slot configuration. The one-slot configuration is not supported by the Security MCU at the moment!

Flash the bootloader before the application:

```shell
west flash
```

From Docker:

```shell
# Note: sometimes you must have the debugger already plugged into your computer _before_ launching Docker in order for Docker to see it.
su-exec root west flash
```
