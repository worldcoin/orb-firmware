import math
import sys

# Print sine as a C array declaration

array_size = 100
if len(sys.argv) == 2:
    array_size = int(sys.argv[1])

sine_array = [math.sin(i / array_size * math.pi) for i in range(array_size)]

print(f"// Sine from 0 PI")
print(f"const float SINE_LUT[{array_size}] = {{")
print(*sine_array, sep=", ")
print("};")
