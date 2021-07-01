# Jetson communication

## Frame protocol

`uint16` format is sent in little endian (LSB first).

| Field    | Format   | Description         |
| -------- | -------- | ------------------- |
| Magic    | `uint16` | Equals `0xdead`     | 
| Length   | `uint16` | Length of `Payload` |
| Payload  | `bytes`  | Byte array of the protocol buffer encoded stream |
| Checksum | `uint16` | CRC16 of `payload`. Used to verify that there is no error on the payload bytes.  |

