import argparse
import time

import gpio.ftdi_gpio as ftdi_gpio
import power.profiler as power
import remote.remote as remote

URL = 'ftdi://ftdi:232:B0001FLO/1'
IP = '192.168.1.143'
JETSON_BOOT_WAIT_TIME_S = 80


def wait_loading_bar(seconds):
    for i in range(seconds):
        time.sleep(1)
        print("\r{:.0f}%".format(i / seconds * 100), end='')
    print("\n")


# This is the test plan:
# - Connect the main battery
# - Press button for 2 seconds to turn on the Orb
# - SSH into the Jetson to stress test both main and security microcontrollers with `mcu-util`
# - Check that Orb info is correct (microcontrollers are responding)
# - Use power profiler to power on the DUT (security board)
# - Press button for 15 seconds to turn off the Orb
# - Disconnect battery & wait for capacitors to discharge
# - Measure the current draw when Orb is off using the power profiler (PPK2) & make sure it's low enough
# - optionally plot the current draw
# - optionally repeat
def main():
    # get optional arguments `ftdi` and `ip`
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--ftdi", help="FTDI URL", default=URL)
    parser.add_argument("-i", "--ip", help="Orb IP", default=IP)
    parser.add_argument("-p", "--password", help="Password for worldcoin user on Orb", default="")
    parser.add_argument("--skip-power-on", help="do not power on the Orb", default=False, const=True, nargs="?",
                        type=bool)
    parser.add_argument("--skip-current-consumption", help="do not power on the Orb", default=False, const=True,
                        nargs="?", type=bool)
    parser.add_argument("--plot-current-consumption", help="plot a graph of current consumption", default=False,
                        const=True, nargs="?", type=bool)
    parser.add_argument("--loop", help="run the test indefinitely", default=1,
                        const=-1, nargs="?", type=int)
    args = parser.parse_args()

    gpio = None
    smu = None
    jetson = None

    try:
        # This disconnects the battery
        print("Attempting to connect to the GPIO controller.")
        gpio = ftdi_gpio.FtdiGpio(args.ftdi)
        print("Connected successfully.")
        print("* NOTE: the battery has been disconnect by connecting to the GPIO controller.")
        print(
            "* NOTE: although the battery was disconnected, the super caps may be charged and thus may have prevented "
            "the board from losing power.")
    except Exception as e:
        print(f"❌ Error connecting to FTDI: {e}")

    count = args.loop
    while count != 0:
        # turn on the Orb if FTDI is connected
        # otherwise, consider the Orb as powered on
        if gpio is not None and not args.skip_power_on:
            print("Connecting battery.")
            gpio.connect_battery()
            print("Pressing button to turn on the Orb.")
            gpio.button_turn_on()
            print(f"Waiting {JETSON_BOOT_WAIT_TIME_S} seconds for Jetson to boot.")
            wait_loading_bar(JETSON_BOOT_WAIT_TIME_S)
            print("Done waiting.")
        elif args.skip_power_on:
            print("Skipping power-on step")
            # make sure the battery is set to a connected state
            gpio.connect_battery()
        else:
            print("⚠️ No FTDI GPIO found, cannot turn on the Orb. Considering the Orb as powered on.")

        try:
            print(f"Attempting to SSH into the Jetson at {args.ip}.")
            jetson = remote.Remote(args.ip, args.password)
            print("Connected successfully.")
        except Exception as e:
            print(f"❌ Error connecting to Jetson: {e}.")
            return 1

        # stress test communication between Jetson and microcontrollers
        print("Beginning stress test of main MCU.")
        jetson.stress_test("main")
        print("Done stress testing main MCU.")
        print("Beginning stress test of security MCU.")
        jetson.stress_test("security")
        print("Done stress testing security MCU.")
        jetson.info()
        jetson = None

        if smu is None and not args.skip_current_consumption:
            try:
                print("Attempting to connect to the power profiler.")
                smu = power.PowerProfiler()
                print("Connected successfully.")
            except Exception as e:
                print("⚠️ Error connecting to source-measurement unit: {}".format(e))
                return 1

        # turn on the DUT if SMU is connected
        if smu is not None:
            smu.dut_power_on()

        # turn off the Orb if FTDI is connected
        # otherwise, ask the user to manually turn off the Orb
        time_to_discharge_seconds = 240
        if gpio is not None:
            print("💤 Turn off the Orb")
            gpio.button_turn_off()
            gpio.disconnect_battery()
        else:
            print(
                "⚠️ No FTDI GPIO found, please manually turn off the Orb and disconnect the battery.\n"
                " Press Enter when done")
            text = input()
            if "skip" in text:
                print("⚠️ Skipping battery removal step")
                time_to_discharge_seconds = 0

        if args.skip_current_consumption:
            print("⚠️ Skipping current consumption measurement step")
            continue

        # wait for the super caps to discharge
        for i in range(time_to_discharge_seconds):
            if i == 0:
                print("🪫 Waiting for super-caps to discharge")
            try:
                gpio.button_turn_on(0.4)
                time.sleep(0.1)
                gpio.button_turn_on(0.4)
                time.sleep(0.1)
                print("\r{:.0f}%".format(i / time_to_discharge_seconds * 100), end='')
            except Exception as e:
                print(f"❌ Error connecting to FTDI: {e}")
                break

        if smu is not None:
            print("🟢 Starting measurement of the security board current draw...")
            smu.start_measuring()
            wait_loading_bar(60)
            consumption_ua = smu.stop_measuring()
            print("📈 Security board current draw: {:.2f} μA".format(consumption_ua))
            if args.plot_current_consumption:
                smu.plot_power_consumption()
            smu.reset()
            assert consumption_ua < 250.0, "Security board current draw is too high"
        else:
            print("⚠️ No SMU found, cannot measure security board current draw")
            return 1

        smu = None

        if count > 0:
            count = count - 1
            print("🔁 Looping {}/{}".format(count, args.loop))
        elif count < 0:
            print("🔁 Looping indefinitely")
    return 0


if __name__ == "__main__":
    ret = main()
    exit(ret)
