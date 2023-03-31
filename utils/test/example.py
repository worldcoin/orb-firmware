
"""
Basic usage of PPK2 Python API.
The basic ampere mode sequence is:
1. read modifiers
2. set ampere mode
3. read stream of data
"""
import time
from src.ppk2_api.ppk2_api import PPK2_API, PPK2_MP
from engineering_notation import EngNumber

ppk2s_connected = PPK2_API.list_devices()
if(len(ppk2s_connected) == 1):
    ppk2_port = ppk2s_connected[0]
    print(f'Found PPK2 at {ppk2_port}')
else:
    print(f'Too many connected PPK2\'s: {ppk2s_connected}')
    exit()

ppk2_test = PPK2_MP(ppk2_port, timeout=1, write_timeout=1, exclusive=True)
ppk2_test.get_modifiers()
voltage = 4.2
ppk2_test.set_source_voltage(voltage * 1000)

ppk2_test.use_source_meter()  # set source meter mode
ppk2_test.toggle_DUT_power("ON")  # enable DUT power

ppk2_test.start_measuring()  # start measuring

delay = 5
print(f"Waiting {delay} seconds to enter low power mode...")
for i in range(delay, 0, -1):
    print(f"{i}")
    time.sleep(1)

# measurements are a constant stream of bytes
# the number of measurements in one sampling period depends on the wait between serial reads
# it appears the maximum number of bytes received is 1024
# the sampling rate of the PPK2 is 100 samples per millisecond
power_sum = 0
current_sum = 0
count = 0

start_time = time.perf_counter()

while True:
    read_data = ppk2_test.get_data()
    if read_data != b'':
        samples = ppk2_test.get_samples(read_data)
        avg_i = sum(samples) / len(samples) / 1_000_000
        avg_i_str = str(EngNumber(avg_i))
        diff = time.perf_counter() - start_time
        print(f"Time elapsed:             {int(diff / 60):02}:{int(diff % 60):02}:{int((diff - int(diff)) * 100):02}")
        print(f"Voltage:                    {voltage}  V")
        print(f"Instantaneous current: {avg_i_str:>10}A")
        power = avg_i * voltage
        power_str = str(EngNumber(power))
        print(f"Instantaneous power:   {power_str:>10}W")

        count += 1

        current_sum += avg_i
        avg_current = current_sum / float(count)
        avg_current_str = str(EngNumber(avg_current))
        print(f"Average current:       {avg_current_str:>10}A")

        power_sum += power
        avg_power = power_sum / float(count)
        avg_power_str = str(EngNumber(avg_power))
        print(f"Average power:         {avg_power_str:>10}W")

        print()
    time.sleep(0.1)
