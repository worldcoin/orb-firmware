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
├── orbs                # our source code
│   ├── app             # main application firmware
│   ├── bootloader      # bootloader firmware
│   ├── common          # common to any module in this directory
│   └── factory_tests   # factory tests firmware             
└── utils               # tools to make things easier, clearer, smarter :) 
    ├── cmake           # cmake scripts
    └── ...
```