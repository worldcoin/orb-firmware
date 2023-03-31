#!/usr/bin/env python3
import argparse
import time
from pyftdi.gpio import GpioAsyncController



# Structure to keep track of the state of the GPIO pins for the Orb
class FtdiGpio:
    ButtonBit = 0
    MainBatteryBit = 1

    def __init__(self, ftdi_url):
        self.gpio = 0
        self.controller = GpioAsyncController()
        self.controller.configure(ftdi_url, direction=0xff)
        self.controller.write(self.gpio)

    def connect_battery(self):
        self.gpio |= 1 << self.MainBatteryBit
        self.controller.write(self.gpio)

    def disconnect_battery(self):
        self.gpio &= ~(1 << self.MainBatteryBit)
        self.controller.write(self.gpio)

    def button_turn_on(self):
        if self.gpio & (1 << self.MainBatteryBit) == 0:
            raise Exception("Cannot press button without battery connected")

        # press button for 2 seconds
        self.gpio |= 1 << self.ButtonBit
        self.controller.write(self.gpio)
        time.sleep(2.0)
        self.gpio &= ~(1 << self.ButtonBit)
        self.controller.write(self.gpio)

    def button_turn_off(self):
        if self.gpio & (1 << self.MainBatteryBit) == 0:
            raise Exception("Cannot press button without battery connected")

        # press button for 15 seconds
        self.gpio |= 1 << self.ButtonBit
        self.controller.write(self.gpio)
        time.sleep(15.0)
        self.gpio &= ~(1 << self.ButtonBit)
        self.controller.write(self.gpio)
