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
        udev \
        wget \
        xz-utils

RUN cd /tmp && \
    wget https://apt.kitware.com/kitware-archive.sh && \
    bash kitware-archive.sh && \
    rm kitware-archive.sh

RUN apt-get update && apt-get install -y cmake

RUN pip3 install \
    pre-commit \
    west

ARG BASE_URL=https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/scripts
RUN cd /tmp && \
    wget -nv \
        "$BASE_URL"/requirements.txt \
        "$BASE_URL"/requirements-base.txt \
        "$BASE_URL"/requirements-build-test.txt \
        "$BASE_URL"/requirements-doc.txt \
        "$BASE_URL"/requirements-run-test.txt \
        "$BASE_URL"/requirements-extras.txt \
        "$BASE_URL"/requirements-compliance.txt && \
    pip3 install -r requirements.txt && \
    rm \
        requirements.txt \
        requirements-base.txt \
        requirements-build-test.txt \
        requirements-doc.txt \
        requirements-run-test.txt \
        requirements-extras.txt \
        requirements-compliance.txt

RUN pyocd pack install stm32g474vetx

# Download and install Zephyr SDK
ARG SDK_VER=0.13.1
RUN cd /tmp && \
    ARCH="$(uname -m)" && \
    wget -nv https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VER}/zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run && \
    chmod +x zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run && \
    ./zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run -- -d /opt/zephyr-sdk-${SDK_VER} && \
    rm ./zephyr-toolchain-arm-${SDK_VER}-linux-${ARCH}-setup.run

RUN cd /tmp && \
    git clone https://github.com/ncopa/su-exec.git && \
    cd su-exec && \
    make && \
    cp -v ./su-exec /usr/local/bin && \
    chmod +s /usr/local/bin/su-exec && \
    rm -rf /tmp/su-exec
