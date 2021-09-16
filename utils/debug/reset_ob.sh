#!/bin/sh

if ! type STM32_Programmer_CLI &> /dev/null; then
  # install foobar here
  echo "🔴 Please add STM32_Programmer_CLI to your PATH"
  exit 1
fi

STM32_Programmer_CLI --connect port=SWD freq=24000 reset=HWrst --optionbytes RDP=0xBB PCROP_RDP=0x1 
STM32_Programmer_CLI --connect port=SWD freq=24000 reset=HWrst --optionbytes RDP=0xAA DBANK=0x0 WRP1A_STRT=0x1 WRP1A_END=0x0 --erase all
