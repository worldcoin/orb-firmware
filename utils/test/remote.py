#!/usr/bin/env python3

import argparse
import threading

import fabric


def mcu_util_stress(conn, mcu):
    assert conn.run("/mnt/ssd/mcu_util stress can-fd --duration 20 {}".format(mcu)).return_code == 0
    assert conn.run("/mnt/ssd/mcu_util stress small-iso-tp --duration 20 {}".format(mcu)).return_code == 0
    assert conn.run("/mnt/ssd/mcu_util stress uart --duration 20 {}".format(mcu)).return_code == 0


def stress_test(conn, mcu):
    t = threading.Thread(target=mcu_util_stress, args=(conn, mcu))
    t.start()
    uart_test_status = False
    result = conn.run("/mnt/ssd/mcu_util dump --duration 60 {}".format(mcu), hide=True, warn=True, pty=True, echo=False)
    for line in result.stdout.splitlines():
        if r"My heart is beating" in line:
            print("✅ UART messages have been received")
            uart_test_status = True
        else:
            print(line)
    t.join()
    assert uart_test_status, "UART test failed"
    print("✅ {} microcontroller stress tests passed".format(mcu))


# Connects to the Orb using fabric
# and run some `mcu_util` commands to test the remote Orb.
def main():
    # get arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("ip", help="IP address of the Orb")
    args = parser.parse_args()

    # Connect to the Orb using fabric (ssh)
    conn = fabric.Connection("worldcoin@{}".format(args.ip))
    # Check mcu_util version
    version = conn.run("/mnt/ssd/mcu_util --version").stdout.strip().split(" ")[-1]
    # make sure version is higher than semver encoded version 0.5.0
    # so that we can use the stress tests correctly
    assert version >= "0.5.0", "mcu_util version is too old, please update"

    # run stress tests on both mcu
    stress_test(conn, "main")
    stress_test(conn, "security")

    # make sure mcu are still responding by asking for info
    cmd = conn.run("/mnt/ssd/mcu_util info")
    assert cmd.return_code == 0
    assert "Main microcontroller's firmware:" in cmd.stdout
    assert "Security microcontroller's firmware:" in cmd.stdout


# default python entry point with arguments
if __name__ == "__main__":
    main()
