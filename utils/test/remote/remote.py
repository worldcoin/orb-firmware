#!/usr/bin/env python3

import threading

import fabric


class Remote:
    def __init__(self, ip):
        self.conn = fabric.Connection("worldcoin@{}".format(ip), connect_timeout=10)
        version = self.conn.run("/mnt/ssd/mcu_util --version", hide=True).stdout.strip().split(" ")[-1]
        assert version >= "0.5.0", "mcu_util version is too old, please update"

    def __del__(self):
        self.conn.close()

    def stress_test(self, mcu):
        t = threading.Thread(target=self.mcu_util_stress, args=(mcu,))
        t.start()
        uart_test_status = False
        result = self.conn.run("/mnt/ssd/mcu_util dump --duration 60 {}".format(mcu), hide=True, warn=True, pty=True,
                               echo=False)
        print_line_count = 0
        for line in result.stdout.splitlines():
            if r"My heart is beating" in line:
                print("❤️ UART messages have been received")
                uart_test_status = True
            elif "Some(Log" in line:
                print_line_count = 5
                print("👀 New log message received:")
                print(line)
            elif print_line_count > 0:
                print(line)
                print_line_count -= 1
        t.join()
        assert uart_test_status, "❌ UART test failed: log message not detected"
        print("✅  {} microcontroller stress tests passed".format(mcu))

    def mcu_util_stress(self, mcu):
        if mcu == "main":
            assert self.conn.run("/mnt/ssd/mcu_util stress uart --duration 20 {}".format(mcu),
                                     timeout=30).return_code == 0
        assert self.conn.run("/mnt/ssd/mcu_util stress can-fd --duration 20 {}".format(mcu),
                             timeout=30).return_code == 0
        assert self.conn.run("/mnt/ssd/mcu_util stress small-iso-tp --duration 20 {}".format(mcu),
                             timeout=30).return_code == 0

    def info(self):
        cmd = self.conn.run("/mnt/ssd/mcu_util info")
        assert cmd.return_code == 0
        assert "Main microcontroller's firmware:" in cmd.stdout
        assert "Security microcontroller's firmware:" in cmd.stdout
