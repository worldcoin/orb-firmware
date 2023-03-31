#!/usr/bin/env python3
import argparse
import time
from pyftdi.gpio import GpioAsyncController

URL = 'ftdi://ftdi:232:B0001FLO/1'

def main():
    # get arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("ftdi", help="FTDI URL", nargs='?', default=URL)
    args = parser.parse_args()

    controller = GpioAsyncController()
    controller.configure(URL, direction=0xff)
    for i in range(512 + 1):
        controller.write(~i & 0xff)
        time.sleep(0.001)


if __name__ == '__main__':
    main()
