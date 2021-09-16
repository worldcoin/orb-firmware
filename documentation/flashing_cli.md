# Flashing target from CLI 

## Install STM32CubeProgrammer

Get your own at https://www.st.com/en/development-tools/stm32cubeprog.html.

## Option bytes

Some protections are enabled when SBSFU runs and they need to be reset before reflashing a new binary.

The protections are set using Option Bytes and can be reset using:

```shell
STM32_Programmer_CLI --connect port=SWD --optionbytes RDP=0xAA WRP1A_STRT=0x1 WRP1A_END=0x2 --erase all
```

On MacOS, the PATH environment variable needs to be extended with: `/Applications/STMicroelectronics/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/`

Checkout [reset_ob.sh](../utils/debug/reset_ob.sh) for a simple way to reset the target.
