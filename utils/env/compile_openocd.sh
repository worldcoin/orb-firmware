#!/usr/bin/env bash

git clone git@github.com:zephyrproject-rtos/openocd.git --recurse-submodules
cd openocd
if [ "$(uname)" == "Darwin" ]; then
    brew install libtool automake libusb texinfo
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    sudo apt-get install make libtool pkg-config autoconf automake texinfo libusb-1.0-0-dev
fi
./bootstrap
./configure --disable-werror
make
# openocd is in src/
# to install in /usr/local/bin/openocd, run
sudo make install
