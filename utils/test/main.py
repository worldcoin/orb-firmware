import argparse
import time

import gpio.ftdi_gpio as ftdi_gpio
import power.profiler as power
import remote.remote as remote

URL = 'ftdi://ftdi:232:B0001FLO/1'
IP = '192.168.1.143'


def wait_loading_bar(seconds):
    for i in range(seconds):
        time.sleep(1)
        print("\r{:.0f}%".format(i / seconds * 100), end="️")
    print("\n")


# This is the test plan:
# - Connect the main battery
# - Press button for 2 seconds to turn on the Orb
# - Use the remote to stress test both main and security microcontrollers
# - Check that microcontroller info is correct
# - Use power profiler to power on the DUT
# - Press button for 15 seconds to turn off the Orb
# - Disconnect battery
# - Measure the current draw when Orb is off using the power profiler (PPK2)
def main():
    # get optional arguments `ftdi` and `ip`
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--ftdi", help="FTDI URL", default=URL)
    parser.add_argument("-i", "--ip", help="Orb IP", default=IP)
    args = parser.parse_args()

    gpio = None
    smu = None
    jetson = None
    try:
        gpio = ftdi_gpio.FtdiGpio(args.ftdi)
    except Exception as e:
        print("❌ Error connecting to FTDI: {}".format(e))

    # turn on the Orb if FTDI is connected
    # otherwise, consider the Orb as powered on
    if gpio is not None:
        gpio.connect_battery()
        gpio.button_turn_on()
        wait_loading_bar(60)
    else:
        print("⚠️ No FTDI GPIO found, cannot turn on the Orb. Considering the Orb as powered on.")

    try:
        jetson = remote.Remote(args.ip)
        jetson = None
    except Exception as e:
        print("❌ Error connecting to Jetson: {}".format(e))

    # stress test communication between Jetson and microcontrollers
    if jetson is not None:
        jetson.stress_test("main")
        jetson.stress_test("security")
        jetson.info()

    try:
        smu = power.PowerProfiler()
    except Exception as e:
        print("⚠️ Error connecting to source-measurement unit: {}".format(e))

    # turn on the DUT if SMU is connected
    if smu is not None:
        smu.dut_power_on()

    # turn off the Orb if FTDI is connected
    # otherwise, ask the user to manually turn off the Orb
    if gpio is not None:
        print("💤 Turn off the Orb")
        gpio.button_turn_off()
        gpio.disconnect_battery()
        print("🪫 Waiting for supercaps to discharge")
        wait_loading_bar(60)
    else:
        print(
            "⚠️ No FTDI GPIO found, please manually turn off the Orb and disconnect the battery.\n"
            " Press Enter when done")
        text = input()
        if "skip" in text:
            print("⚠️ Skipping battery removal step")
        else:
            print("🪫 Waiting for supercaps to discharge")
            wait_loading_bar(60)

    if smu is not None:
        print("🟢 Starting measurement of the security board current draw...")
        smu.start_measuring()
        wait_loading_bar(60)
        consumption_ua = smu.stop_measuring()
        print("📈 Security board current draw: {:.2f} μA".format(consumption_ua))
        smu.plot_power_consumption()
        smu.reset()
        assert consumption_ua < 250.0, "Security board current draw is too high"
    else:
        print("⚠️ No SMU found, cannot measure security board current draw")


if __name__ == "__main__":
    main()
