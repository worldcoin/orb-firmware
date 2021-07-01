#!/usr/bin/env python
import getopt
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

    while 1:
        data_bytes = bytearray([0x00, 0x00])

        # looking for magic
        while data_bytes[len(data_bytes)-2] != 0xad and data_bytes[len(data_bytes)-1] != 0xde:
            data_bytes.append(ser.read(1)[0])

        # next 2 bytes are the payload size
        size = int.from_bytes(ser.read(2), byteorder='little', signed=False)
        payload = ser.read(size)

        print('New frame, size: {}\tpayload: {}'.format(size, payload))

        computed_crc = 0xffff
        computed_crc = crc16.crc16xmodem(payload, computed_crc)

        read_crc = int.from_bytes(ser.read(2), byteorder='little', signed=False)

        if read_crc != computed_crc:
            print("Error: CRC miscmatch")
        else:
            # parse payload
            print("Parsing...")

if __name__ == '__main__':
    main(sys.argv)
