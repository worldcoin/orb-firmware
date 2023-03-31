from ppk2_api.ppk2_api import PPK2_API, PPK2_MP

# Constants
VOLTAGE = 3.7

class PowerProfiler:
    def __init__(self):
        self.samples = []
        ppk2s_connected = PPK2_API.list_devices()
        if len(ppk2s_connected) == 1:
            self.port = ppk2s_connected[0]
            self.ppk = PPK2_MP(self.port, buffer_max_size_seconds=60, timeout=1, write_timeout=1, exclusive=True)
            self.ppk.get_modifiers()
            self.ppk.set_source_voltage(VOLTAGE * 1000)
            self.ppk.use_source_meter()
        else:
            raise Exception(f'Too many connected PPK2\'s: {ppk2s_connected}')

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
            avg = sum(self.samples) / len(self.samples)
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
