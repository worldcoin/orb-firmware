#usr/bin/env bash

# before running this script, make sure that mcu-util is installed
# and that the binaries are in /mnt/scratch with A and B versions:
# - /mnt/scratch/app_mcu_sec_$VERSION.signed.encrypted.bin

counter=0
A_VERSION=1.1.5
B_VERSION=1.1.6

# infinite loop
while true; do
  counter=$((counter + 1))

  # grep security MCU version from mcu-util info output: "Security microcontroller's firmware: v1.1.5-0x379dbea1"
  version=$(mcu-util info | grep "Security microcontroller's firmware:" | awk '{print $9}' | sed 's/v//g' | sed 's/-.*//g' | sed 's/0x//g')

  # check that version contains 1.1.5
  if [[ "$version" == "$A_VERSION" ]]; then
      mcu-util image update /mnt/scratch/app_mcu_sec_$B_VERSION.signed.encrypted.bin security
      sleep 30
      update_version=$(mcu-util info | grep "Security microcontroller's firmware:" | awk '{print $9}' | sed 's/v//g' | sed 's/-.*//g' | sed 's/0x//g')
      if [[ "$update_version" == "$B_VERSION" ]]; then
          echo "PASS $counter"
      else
          echo "FAIL $counter: $update_version != $B_VERSION"
      fi
  elif [[ "$version" == "$B_VERSION" ]]; then
      mcu-util image update /mnt/scratch/app_mcu_sec_$A_VERSION.signed.encrypted.bin security
      sleep 30
      update_version=$(mcu-util info | grep "Security microcontroller's firmware:" | awk '{print $9}' | sed 's/v//g' | sed 's/-.*//g' | sed 's/0x//g')
      if [[ "$update_version" == "$A_VERSION" ]]; then
          echo "PASS $counter"
      else
          echo "FAIL $counter: $update_version != $A_VERSION"
      fi
  else
      echo "FAIL $counter: unknown version: $version"
      return 1
  fi

  if [[ "$counter" == "50" ]]; then
      echo "Test is over, exiting..."
      return 0
  fi
done
