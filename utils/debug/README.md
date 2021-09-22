# Debug

## Logs

Logs are printed out on the UART port. 

Configuration is: 
  - 115200 8N1
  - TX pin: `PC10`
  - RX pin: `PC11` ⚠️ not used: incoming data is not parsed yet
  - On STM32G-DISCO board, you can use the Virtual COM port (STLink USB).

### Python scripts

Using the Python script to print logs:

```shell
$ python uart_dump.py -p /dev/tty.usbmodem22203 -b 115200

🎧 Listening UART (8N1 115200) on /dev/tty.usbmodem22203
🟢 [orb/app/main.c:89] 🤖
🟢 [orb/app/main.c:90] Firmware v0.0.1, hw:2
🟢 [orb/app/main.c:95] Reset reason: EXTERNAL_RESET_PIN_RESET
```

## OpenOCD

A [config file for the STM32G4-discovery board](stm32g4discovery.cfg) is ready to be used with OpenOCD v0.11.0.
