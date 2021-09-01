# main-microcontroller

Code for the main microcontroller

## 🧭 Repository structure

```shell
.
├── CMakeLists.txt      # root CMakeLists
├── external            # external libraries
│   ├── nanopb          # Protobuf support
│   ├── freertos        # FreeRTOS from mainline repository
│   └── ...
├── components          # STM32 SDK
│   ├── Drivers         # HAL drivers
│   └── ...             
├── orb                # our source code
│   ├── app             # main application firmware
│   ├── bootloader      # bootloader firmware
│   ├── common          # common to any module in this directory
│   └── factory_tests   # factory tests firmware             
└── utils               # tools to make things easier, clearer, smarter :) 
    ├── cmake           # cmake scripts
    └── ...
```

## Getting started

### Conda

You can use Conda to get all the dependencies in an isolated environment. Checkout the available [environment](utils/env/environment.yml).

#### Conda installation

```shell
# Download for Mac:
$ wget https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh
# Download for Linux:
$ wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh
# install it
$ bash Miniconda3-latest-*-x86_64.sh
```

#### Environment

To install the environment:

```shell
conda env create -f utils/env/environment.yml
```

In case you need to update to latest environment:

```shell
conda env update -f utils/env/environment.yml
```

Then activate:

```shell
conda activate worldcoin
```

### Flash target

```shell
make flash stm32g4discovery
```

### Debug

> 💡 [More details](utils/debug/README.md).

#### Printing logs

Pass the port to listen to instead of `/dev/tty.usbmodem22203` in the line below:

```shell
make logs /dev/tty.usbmodem22203
```
