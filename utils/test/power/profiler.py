import time

from ppk2_api.ppk2_api import PPK2_API, PPK2_MP

# Constants
VOLTAGE_MV = 3700  # mV


class PowerProfiler:
    def __init__(self):
        self.samples = []
        ppk2s_connected = PPK2_API.list_devices()
        if len(ppk2s_connected) == 1:
            self.port = ppk2s_connected[0]
            self.ppk = PPK2_MP(self.port, buffer_max_size_seconds=60, timeout=1, write_timeout=1, exclusive=True)
            self.ppk.get_modifiers()
            self.ppk.set_source_voltage(VOLTAGE_MV)
            self.ppk.use_source_meter()
        else:
            raise Exception(f'Expected 1 PPK2 to be connected, found {len(ppk2s_connected)}: {ppk2s_connected}')

    def __del__(self):
        self.ppk = None

    def dut_power_on(self):
        self.ppk.toggle_DUT_power("ON")

    def dut_power_off(self):
        self.ppk.toggle_DUT_power("OFF")

    def start_measuring(self):
        self.ppk.start_measuring()

    def stop_measuring(self):
        read_data = self.ppk.get_data()
        self.ppk.stop_measuring()
        if read_data != b'':
            self.samples = self.ppk.get_samples(read_data)
            avg = sum(self.samples[0]) / len(self.samples[0])
            return avg
        else:
            raise Exception("No data read from PPK2")

    def plot_power_consumption(self):
        import matplotlib.pyplot as plt
        plt.plot(self.samples)
        plt.ylabel('Current (μA)')
        plt.xlabel('Samples')
        plt.show()

    def reset(self):
        self.samples = []


def main():
    import argparse
    p = argparse.ArgumentParser(
        description=f"Measure current consumption of the security board using the PPK2.")
    p.add_argument("-d", "--duration", default=20,
                   help="Duration of measurement in seconds. Defaults to 20.")
    args = p.parse_args()
    profiler = PowerProfiler()
    profiler.dut_power_on()
    profiler.start_measuring()
    time.sleep(args.duration)
    avg = profiler.stop_measuring()
    print(f'Average current consumption: {avg} μA')


if __name__ == "__main__":
    main()
