#!/usr/bin/env python
import getopt
import random

from mcu_messaging_pb2.mcu_messaging_pb2 import McuMessage, Version, JetsonToMcu
from serial import Serial
import sys
import crc16

def send_data(ser):
    output_data = McuMessage()
    output_data.version = Version.VERSION_0

    value = random.randrange(255)
    print("Sending brightness: {}".format(value))
    output_data.j_message.brightness_front_leds.white_leds = value

    bytes_to_send = output_data.SerializeToString()
    computed_crc = 0xffff
    computed_crc = crc16.crc16xmodem(bytes_to_send, computed_crc)
    size = len(bytes_to_send)
    bytes_to_send = size.to_bytes(2, 'little') + bytes_to_send
    bytes_to_send = bytearray(b'\xad\xde') + bytes_to_send
    bytes_to_send = bytes_to_send + computed_crc.to_bytes(2, 'little')
    ser.write(bytes_to_send)

def main(argv):
    baud_rate = 115200
    port = ""
    help = "Usage: {} -p <port> [-b <baud_rate>] \nDefault baud rate: 115200".format(
        sys.argv[0])

    if len(sys.argv) < 2:
        print(help)
        quit()

    try:
        opts, args = getopt.getopt(argv[1:], "p:b:h", ["port="])

        if opts != None:
            for opt, arg in opts:
                if opt == '-h':
                    print(help)
                    sys.exit()
                elif opt in ("-p", "--port"):
                    port = arg
                elif opt in "-b":
                    baud_rate = int(arg)
    except getopt.GetoptError:
        print(help)

    ser = Serial(port, 115200)
    print("🎧 Listening UART (8N1 {}) on {}".format(baud_rate, port))

    data = McuMessage()



    while 1:
        data_bytes = bytearray([0x00, 0x00])

        # looking for magic
        while data_bytes[len(data_bytes)-2] != 0xad and data_bytes[len(data_bytes)-1] != 0xde:
            data_bytes.append(ser.read(1)[0])

        # next 2 bytes are the payload size
        size = int.from_bytes(ser.read(2), byteorder='little', signed=False)

        # read payload
        payload = ser.read(size)

        # compute CRC over payload
        computed_crc = 0xffff
        computed_crc = crc16.crc16xmodem(payload, computed_crc)

        # read CRC from frame
        read_crc = int.from_bytes(ser.read(2), byteorder='little', signed=False)

        # parse payload if CRCs match
        if read_crc != computed_crc:
            print("Error: CRC mismatch, rejecting frame")
        else:
            print('New frame, size: {}\tCRC ✅\t\tpayload: {}'.format(size, payload))

            # parse payload
            data.ParseFromString(payload)
            print("{}".format(data))

        send_data(ser)

if __name__ == '__main__':
    main(sys.argv)
