# Title: pyOCD user script for STM32G4
# Description:  This script is used to enable the trace clock for STM32G4
#               when the device is sleeping (standby, stop and sleep modes).
# Usage:        pyocd flash -M under-reset --target stm32g474vetx --script pyocd_user.py [--verbose] <mcu-fw-image.hex>

DBGMCU_CR = 0xE0042004


def will_init_target(target, init_sequence):
    def set_traceclken():
        info("Allowing debug when microcontroller is sleeping (standby, stop or sleep modes)")
        # allow debug in standby, stop and sleep modes
        dbgmcr_cr_val = 0x00700007
        # see https://developer.arm.com/documentation/ddi0439/b/Debug/About-the-AHB-AP/AHB-AP-programmers-model?lang=en
        # for more information about the AHB-AP registers
        target.dp.write_ap(0x00, 0x23000052)  # write CSW register (offset 0x0) of AP0
        target.dp.write_ap(0x04, DBGMCU_CR)  # write TAR register (offset 0x04) of AP0
        target.dp.write_ap(0x0C, dbgmcr_cr_val)  # write DRW register (offset 0x0C) of AP0 with DBGMCR_CR value

    init_sequence.insert_after('dp_init', ('set_traceclken', set_traceclken))
