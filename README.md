# proto2-firmware

West workspace for proto2 firmware

## Setting Up Development Environment

You can set up your development environment directly on your machine or you may use the provided Docker image.
The [Zephyr getting started guide](https://docs.zephyrproject.org/latest/getting_started/index.html)
only has instructions for Ubuntu, MacOS, and Windows.
For this guide's description of setting up a native build environment
(i.e., not using a Docker container), we assume your host machine is running
Ubuntu 20.04.
If you are not using Ubuntu 20.04, your mileage may vary.
Note that all of these steps assume that you have an SSH key set up properly
with Github and that you have access to all of the MCU repos.
These repositories are enumerated in the [west.yml](west.yml) file.

### Generic Steps

1. Install the `west` meta-tool.
   ```shell
   pip3 install west
   ```
2. Create an empty directory where we will put our projects and dependencies. We assume that you set an environmental variable called `REPO_DIR`. I like to set this to `$HOME/firmware`.

   ```shell
   export REPO_DIR=$HOME/firmware # This env var is used inside the Docker Makefile, hence the need for `export`
   mkdir "$REPO_DIR"
   ```

3. Clone the manifest repository using west.

   ```shell
   cd "$REPO_DIR"
   west init -m git@github.com:worldcoin/proto2-main-mcu.git --mr main
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

A reminder that these steps assume your host machine is running Ubuntu 20.04.
These instructions are mainly just an adaptation of the instructions in the
[Zephyr getting started guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

5. Follow these instructions for [installing dependencies](https://docs.zephyrproject.org/latest/getting_started/index.html#install-dependencies).

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

#### Finally, to Build:

```shell
cd "$REPO_DIR"/orb/main_board/app/
west build
```

#### To Flash:

```shell
cd "$REPO_DIR"/orb/main_board/app/
west flash
```

if in the Docker container:

```shell
cd "$REPO_DIR"/orb/main_board/app/
su-exec root west flash
```

## Contributing

### Formatting

```shell
cd "$REPO_DIR"/orb/main_board/app/
pre-commit install -c utils/format/pre-commit-config.yaml
```
