# main-microcontroller

Code for the main microcontroller

## 🧭 Repository structure

```shell
.
├── CMakeLists.txt      # root CMakeLists
├── external            # external libraries
│   ├── nanopb          # Protobuf support
│   └── ...
├── components          # STM32 SDK
│   ├── Drivers         # HAL drivers
│   ├── Middlewares     # Middlewares such as FreeRTOS
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

You can use Conda to get all the dependencies in an isolated environment. Checkout the available [environment](utils/environment.yml).

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
conda env create -f utils/environment.yml
```

Then activate:

```shell
conda activate worldcoin
```
