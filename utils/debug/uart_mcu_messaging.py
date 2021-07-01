#!/usr/bin/env python
import getopt

from mcu_messaging_pb2.mcu_messaging_pb2 import DataHeader
from serial import Serial
import sys
import crc16

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

    data = DataHeader()

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

if __name__ == '__main__':
    main(sys.argv)
