FROM ubuntu:22.04

# Set default shell during Docker image build to bash
SHELL ["/bin/bash", "-c"]

ARG WGET_ARGS="-q --show-progress --progress=bar:force:noscroll --no-check-certificate"
ARG PROTOC_VERSION=25.2
ARG SDK_VERSION=0.16.5

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
        libusb-dev \
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
    	  unzip \
        xz-utils

RUN cd /tmp && \
    ARCH="$(uname -m)" && \
    wget https://github.com/Kitware/CMake/releases/download/v3.24.2/cmake-3.24.2-linux-${ARCH}.sh && \
    bash cmake-3.24.2-linux-${ARCH}.sh --skip-license --prefix=/usr/ && \
    rm cmake-3.24.2-linux-${ARCH}.sh

RUN apt-get update && apt-get install -y cmake

RUN pip3 install \
    junit2html \
    junitparser \
    pre-commit \
    pyftdi \
    ppk2-api \
    fabric2 \
    west

ARG BASE_URL=https://raw.githubusercontent.com/worldcoin/zephyr/develop/scripts
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
RUN ARCH="$(uname -m)" && \
    wget ${WGET_ARGS} -O /tmp/sdk.tar.xz https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VERSION}/zephyr-sdk-${SDK_VERSION}_linux-${ARCH}_minimal.tar.xz && \
    tar -xf /tmp/sdk.tar.xz -C /opt/ && cd /opt/zephyr-sdk-${SDK_VERSION} && \
    ./setup.sh -t arm-zephyr-eabi -h -c && \
    rm -rf /tmp/sdk.tar.xz && . /opt/zephyr-sdk-${SDK_VERSION}/environment-setup-${ARCH}-pokysdk-linux

# Clean up stale packages
RUN apt-get clean -y && \
	apt-get autoremove --purge -y && \
	rm -rf /var/lib/apt/lists/*

# Install protobuf-compiler
RUN mkdir -p /opt/protoc && \
	cd /opt/protoc && \
    ARCH="$(uname -m)" && \
	PROTOC_HOSTTYPE=$(case ${ARCH} in x86_64) echo "x86_64";; aarch64) echo "aarch_64";; esac) && \
	wget ${WGET_ARGS} https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VERSION}/protoc-${PROTOC_VERSION}-linux-${PROTOC_HOSTTYPE}.zip && \
	unzip protoc-${PROTOC_VERSION}-linux-${PROTOC_HOSTTYPE}.zip && \
	ln -s /opt/protoc/bin/protoc /usr/local/bin && \
	rm -f protoc-${PROTOC_VERSION}-linux-${PROTOC_HOSTTYPE}.zip

# install su-exec
RUN cd /tmp && \
    git clone https://github.com/ncopa/su-exec.git && \
    cd su-exec && \
    make && \
    cp -v ./su-exec /usr/local/bin && \
    chmod +s /usr/local/bin/su-exec && \
    rm -rf /tmp/su-exec
