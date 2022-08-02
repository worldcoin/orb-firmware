# proto2-firmware

West workspace for microcontrollers' firmware.

This repo contains Firmware for the several microcontrollers present in the Orb:

- ☀️ Main board microcontroller
  - Anything related to board supplies and controlling sensors on the Orb
- 🚨 Security board microcontroller
  - Anything related to security: signing data and anti-fraud sensors

## Setting Up Development Environment

You can set up your development environment directly on your machine, or you may
use the provided Docker image.
The [Zephyr getting started guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
only has instructions for Ubuntu, macOS, and Windows. For this guide's
description of setting up a native build environment (i.e., not using a
Docker container), we assume your host machine is running Ubuntu 20.04/22.04
or macOS. If you are not using one of those, your mileage may vary. Note that
we expect [`brew`](https://brew.sh/) to be available on macOS. Note also that
all of these steps assume that you have an SSH key set up properly with Github
and that you have access to all of the MCU repos. These repositories are
enumerated in the [west.yml](west.yml) file.

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
   west init -m git@github.com:worldcoin/orb-mcu-firmware.git --mr main
   ```

   This will create a directory called `orb`.

4. Now import all projects and dependencies.

   ```shell
   west update
   ```

#### Docker-specific Steps

5. Enter the Docker container to perform your work. (Note: you need to first configure Github to read the [container registry](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry))

   ```shell
   cd "$REPO_DIR"/orb/utils/docker
   make shell
   ```

   You may also build the Docker image locally.
   NOTE: when you build locally, you will get a image with the tag `local`,
   but when you run `make *-build`, Make will use the SHA of the known good
   Docker image used by CI, _unless_ you set the make variable
   `DOCKER_TAG` to something. For instance, do
   `make main_board-build DOCKER_TAG=local` to use the Docker image you built
   locally.
   This locally-built image may not work since Ubuntu-based containers
   are not strictly reproducible.

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

   You may change `INSTALL_LOC` to choose an installation location.
   You may also change `SDK_VER` to choose a specific toolchain version.

   ```shell
   tmp=$(mktemp)
   cat > "$tmp" <<EOF
   #!/bin/bash
   set -ue
   set -o pipefail
   install_zephyr_sdk() {
    INSTALL_LOC=\$HOME
    KERNEL=\$(uname -s | tr '[[:upper:]]' '[[:lower:]]')
    [ "\$KERNEL" = darwin ] && KERNEL=macos
    ARCH=\$(uname -m)
    [ "\$ARCH" = arm64 ] && ARCH=aarch64
    if [ -z "\${SDK_VER-}" ]; then
        if ! SDK_VER=\$(curl -s https://api.github.com/repos/zephyrproject-rtos/sdk-ng/releases/latest \\
            | grep tag_name \\
            | awk '{print substr(\$2, 3, length(\$2)-4)}'); then
            echo "Some error occured when trying to determine the latest SDK version" >&2
            return 1
        fi
    fi
    echo "SDK_VER is '\$SDK_VER'"
    TAR_NAME=zephyr-sdk-\${SDK_VER}_\${KERNEL}-\${ARCH}_minimal.tar.gz
    echo "TAR_NAME is \$TAR_NAME"
    URL=https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v\${SDK_VER}/\${TAR_NAME}
    if [ ! -e "\$INSTALL_LOC/zephyr-sdk-\${SDK_VER}/.__finished" ]; then
        echo "Downloading \${TAR_NAME}..." &&
        ( cd "\$INSTALL_LOC" && curl -JLO "\$URL" ) &&
        echo 'Decompressing...' &&
        (cd "\$INSTALL_LOC" && tar xf "\$TAR_NAME" ) &&
        ( cd "\$INSTALL_LOC"/zephyr-sdk-\${SDK_VER} &&
        ./setup.sh -t arm-zephyr-eabi -h -c ) &&
        rm "\$INSTALL_LOC/\$TAR_NAME" &&
        echo > "\$INSTALL_LOC"/zephyr-sdk-\${SDK_VER}/.__finished
    else
        echo "SDK already exists. Skipping."
    fi
   }
   install_zephyr_sdk
   EOF
   chmod +x "$tmp"
   "$tmp"
   ```

   For the Conda environment:

   ```shell
    # Set the environment variable to point towards the installed toolchain
    # Do no include the trailing `/bin` in GNUARMEMB_TOOLCHAIN_PATH
    conda env config vars set GNUARMEMB_TOOLCHAIN_PATH=/path/to/your/toolchain/
   ```

9. Install udev rules to allow for flashing as a non-root user (For Linux only).

   ```shell
   sudo cp zephyr-sdk-${SDK_VER}/sysroots/x86_64-pokysdk-linux/usr/share/openocd/contrib/60-openocd.rules /etc/udev/rules.d
   sudo udevadm control --reload
   ```

10. Install the protobuf compiler.

    ```shell
    if [ "$(uname -s)" = Darwin ]; then
        brew install protobuf-c
    else
        sudo apt install protobuf-compiler
    fi
    pip3 install protobuf grpcio-tools
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
