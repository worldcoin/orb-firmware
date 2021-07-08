# Software architecture

![Software block diagram](software_block_diagram.png)

## Release notes

Not-tagged for release:

[x] Compilation based on CMake & Makefile
[x] Debug using SWD, ST-Link
[x] Logging over UART, uses DMA
[x] Errors, assertions with debug message
[x] Com module for communication between Jetson and Microcontroller: TX & RX over UART, uses DMA
[x] Serializer/Deserializer to pack/unpack data using Protobuf
[x] Control module in place to parse incoming commands, not taking into account any application state
[x] Data provider to push data to be sent from any other module/task
[ ] Accelerometer data to Jetson, event-based
[ ] Gyroscope data to Jetson, event-based
