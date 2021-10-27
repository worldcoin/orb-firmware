FROM ubuntu:20.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get upgrade -y && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        bzip2 \
        ccache \
        device-tree-compiler \
        dfu-util \
        file \
        gcc \
        git \
        gperf \
        libsdl2-dev \
        make \
        ninja-build \
        protobuf-compiler \
        python3-dev \
        python3-pip \
        python3-setuptools \
        python3-tk \
        python3-wheel \
        wget \
        xz-utils

RUN cd /tmp && \
    wget https://apt.kitware.com/kitware-archive.sh && \
    bash kitware-archive.sh && \
    rm kitware-archive.sh

RUN apt-get update && apt-get install -y cmake

RUN pip3 install -U west

RUN cd /tmp && \
    mkdir zephyr && \
    cd zephyr && \
    git config --global init.defaultBranch main && \
    git init && \
    git remote add origin -f https://github.com/zephyrproject-rtos/zephyr.git && \
    git pull origin main && \
    pip3 install -r scripts/requirements.txt && \
    cd .. && \
    rm -rf zephyr

# Download and install Zephyr SDK
ARG SDK_VER=0.13.1
RUN cd /tmp && \
    ARCH="$(uname -m)" && \
    wget -nv https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VER}/zephyr-sdk-${SDK_VER}-linux-${ARCH}-setup.run && \
    chmod +x zephyr-sdk-${SDK_VER}-linux-${ARCH}-setup.run && \
    ./zephyr-sdk-${SDK_VER}-linux-${ARCH}-setup.run -- -d /opt/zephyr-sdk-${SDK_VER} && \
    rm zephyr-sdk-${SDK_VER}-linux-${ARCH}-setup.run && \
    cd /opt/zephyr-sdk-${SDK_VER} && \
    rm -rf aarch64-zephyr-elf/ arc-zephyr-elf/ arc64-zephyr-elf/ mips-zephyr-elf/ nios2-zephyr-elf/ riscv64-zephyr-elf/ sparc-zephyr-elf/ xtensa/ sysroots
