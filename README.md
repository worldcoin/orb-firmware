# proto2-firmware

West workspace for microcontrollers' firmware.

This repo contains Firmware for the several microcontrollers present in the Orb:

- ☀️ Main board microcontroller
  - Anything related to board supplies and controlling sensors on the Orb
- 🚨 Security board microcontroller
  - Anything related to security: signing data and anti-fraud sensors

## Setting Up Development Environment

You can set up your development environment directly on your machine, or you may use the provided Docker image.
The [Zephyr getting started guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
only has instructions for Ubuntu, MacOS, and Windows. For this guide's description of setting up a native build
environment
(i.e., not using a Docker container), we assume your host machine is running Ubuntu 20.04. If you are not using Ubuntu
20.04, your mileage may vary. Note that all of these steps assume that you have an SSH key set up properly with Github
and that you have access to all of the MCU repos. These repositories are enumerated in the [west.yml](west.yml) file.

### Generic Steps

1. Install the `west` meta-tool.

   ```shell
   pip3 install west
   ```

2. Create an empty directory where we will put our projects and dependencies. We assume that you set an environmental
   variable called `REPO_DIR`. I like to set this to `$HOME/firmware`.

   ```shell
   export REPO_DIR=$HOME/firmware # This env var is used inside the Docker Makefile, hence the need for `export`
   mkdir "$REPO_DIR"
   ```

3. Clone the manifest repository using west.

   ```shell
   cd "$REPO_DIR"
   west init -m git@github.com:worldcoin/proto2-mcu.git --mr main
   ```

   This will create a directory called `orb`.

4. Now import all projects and dependencies.

   ```shell
   west update
   ```

#### Docker-specific Steps

5. Enter the Docker container to perform your work.
   ```shell
   cd "$REPO_DIR"/orb/utils/docker
   make shell
   ```
   You may also build the Docker image locally.
   ```shell
   cd "$REPO_DIR"/orb/utils/docker
   make build
   ```

#### Native-specific Steps

A reminder that these steps assume your host machine is running Ubuntu 20.04. These instructions are mainly just an
adaptation of the instructions in the
[Zephyr getting started guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

5. Install the dependencies

   - Follow these instructions
     for [installing dependencies](https://docs.zephyrproject.org/latest/getting_started/index.html#install-dependencies)
     .
   - Or install the Conda environment provided [here](utils/env/environment.yml). Make sure to set the variables.
     ```shell
     conda env create -f orb/utils/env/environement.yml
     conda activate worldcoin
     ```

6. Export CMake packages.

   ```shell
   cd "$REPO_DIR"
   west zephyr-export
   ```

7. Install additional Python dependencies.

   ```shell
   pip3 install -r "$REPO_DIR"/zephyr/scripts/requirements.txt
   ```

8. Install a toolchain.

   Zephyr provides toolchains for the following host architectures:

   - `aarch64`
   - `x86_64`

   ```shell
   ARCH=$(uname -m)
   SDK_VER=0.13.1
   INSTALL_LOC=$HOME/zephyr-sdk-${SDK_VER} # You can also use /opt instead of $HOME
   wget -P /tmp -nv https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VER}/zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run
   chmod +x /tmp/zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run
   /tmp/zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run -- -d "$INSTALL_LOC"
   rm /tmp/zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run
   ```

   For the Conda environment:

   ```shell
    # Set the environment variable to point towards the installed toolchain
    # Do no include the trailing `/bin` in GNUARMEMB_TOOLCHAIN_PATH
    conda env config vars set GNUARMEMB_TOOLCHAIN_PATH=/path/to/your/toolchain/
   ```

9. Install udev rules to allow for flashing as a non-root user.

   ```shell
   sudo cp "$INSTALL_LOC"/sysroots/x86_64-pokysdk-linux/usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d
   sudo udevadm control --reload
   ```

10. Install the protobuf compiler.

    ```shell
    sudo apt install protobuf-compiler
    ```

11. Install CMSIS Pack for PyOCD

    ```shell
    pyocd pack install stm32g474vetx
    ```

12. You may need to fix up some Python dependencies:
    ```shell
    pip3 install six==1.15.0
    pip3 install pyyaml==6.0
    ```

Your directory structure now looks similar to this:

```
firmware
├── bootloader
├── modules
├── orb
└── zephyr
```

#### Finally, to Build and Flash

See the board-specific docs:

- ☀️ [Main board](main_board/app/README.md)
- 🚨 [Security board](sec_board/app/README.md)

#### Logging

Print out the bootloader and main MCU application logs using:

```shell
# replace /dev/ttyxxx with your UART device
python "$REPO_DIR"/orb/utils/debug/uart_dump.py -p /dev/ttyxxx -b 115200
```

##### Security board logs

UART is not easily accessible on the Security Board 2.x so the default logging backend is using SeggerRTT (logs sent
through the debugger SWD interface). From `pyocd` v0.33.1, printing RTT logs is as simple as:

```shell
pyocd rtt --target=stm32g474vetx
```

## Contributing

### Pre-commit hooks

These pre-commit hooks will be checked in CI, so it behooves you to install them now. This requires the `pre-commit`
python package to be installed like so: `pip3 install pre-commit`.

```shell
cd "$REPO_DIR"/orb && pre-commit install -c utils/format/pre-commit-config.yaml
```

### Check Formatting

Manually:

```shell
cd "$REPO_DIR" && pre-commit run --all-files --config orb/utils/format/pre-commit-config.yaml
```

Using Docker:

```shell
cd "$REPO_DIR"/orb/utils/docker
make format
```
