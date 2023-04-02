#!/usr/bin/env python3
import time
from pyftdi.gpio import GpioAsyncController

# Structure to keep track of the state of the GPIO pins for the Orb
class FtdiGpio:
    # These pins are all active low!
    MainBatteryBit = 0
    ButtonBit = 1

    def __init__(self, ftdi_url):
        self.gpio = 0xff
        self.controller = GpioAsyncController()
        self.controller.configure(ftdi_url, direction=0xff, initial=self.gpio)
        self.controller.write(self.gpio)

    def connect_battery(self):
        self.gpio &= ~(1 << self.MainBatteryBit)
        self.controller.write(self.gpio)
        time.sleep(1)

    def disconnect_battery(self):
        self.gpio |= (1 << self.MainBatteryBit)
        self.controller.write(self.gpio)
        time.sleep(1)

    def button_turn_on(self):
        if self.gpio & (1 << self.MainBatteryBit) > 0:
            raise Exception("Cannot press button without battery connected")

        # press button for 2 seconds
        self.gpio &= ~(1 << self.ButtonBit)
        self.controller.write(self.gpio)
        time.sleep(2.0)
        self.gpio |= (1 << self.ButtonBit)
        self.controller.write(self.gpio)

    def button_turn_off(self):
        if self.gpio & (1 << self.MainBatteryBit) > 0:
            raise Exception("Cannot press button without battery connected")

        # press button for 15 seconds
        self.gpio &= ~(1 << self.ButtonBit)
        self.controller.write(self.gpio)
        time.sleep(15.0)
        self.gpio |= (1 << self.ButtonBit)
        self.controller.write(self.gpio)

def main():
    import argparse
    URL = 'ftdi://ftdi:232:B0001FLO/1'
    p = argparse.ArgumentParser(description=f"Connects to the GPIO controller at {URL} by default and resets all lines, optionally followed by an additional action. This should diconnect the battery, though the supercaps may continue to provide power for some time.")
    p.add_argument("-f", "--ftdi", help="FTDI URL", default=URL)
    p.add_argument("-a", "--action", choices=["reset", "turn-on"], default="reset", help="Action to perform. Defaults to reset.")
    args = p.parse_args()
    gpio = FtdiGpio(args.ftdi)
    if args.action == "turn-on":
        gpio.connect_battery()
        gpio.button_turn_on()


if __name__ == "__main__":
    main()
