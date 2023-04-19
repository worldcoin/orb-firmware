# Automated tests

This directory contains automated tests for the microcontrollers on the Orb.

Some are run on the Orb itself through the `mcu_util` binary.

## SSH setup

We don't want to have to type in a password every time we run a test.
To set this up, run the following command on the host that will trigger the
tests.

```shell
    ssh-copy-id worldcoin@<ip address>
    ssh worldcoin@<ip address> # should work without a password
```

## FTDI setup

Connect the FTDI:

- FTDI TX -> main battery (HOST_PRESENT)
- FTDI RX -> Orb button

## Running tests

A few consideration to take into account:

- The `mcu_util` path is hard-coded at `/mnt/ssd/mcu_util`, so make sure to copy the `mcu_util v0.5.0+` binary to this location.
- In order to test that UART messages are received, a log message is sent from the MCU to the Jetson over CAN.
  This feature of the MCU is optional, so make sure to compile the MCU code with `CONFIG_MCU_UTIL_UART_TESTS=y`.
- SSH is performed with user `worldcoin`.

```shell
python3 main.py [--ftdi=...] [--ip=...]
```
