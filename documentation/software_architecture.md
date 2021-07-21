# Software architecture

![Software block diagram](software_block_diagram.png)

## Release notes

**[2021-07-08]** Not tagged for release:

- [x] Compilation based on CMake & Makefile
- [x] Debug using SWD, ST-Link
- [x] Logging over UART, uses DMA
- [x] Errors, assertions with debug message
- [x] Building from Gitlab Pipelines
  - [x] Keep `elf` artifacts
  - [x] Set up Unit Tests framework
- [x] Communication module for communication between Jetson and Microcontroller: TX & RX over UART, uses DMA
- [x] Serializer/Deserializer to pack/unpack data using Protobuf, thread-safe
- [x] Control module to parse incoming commands
  - [ ] Implement handlers for all the commands
  - [ ] Take into account application state
- [x] Data provider to push data to be sent from any other module/task
  - [x] Accelerometer data to Jetson, event-based
  - [ ] Gyroscope data to Jetson, event-based
