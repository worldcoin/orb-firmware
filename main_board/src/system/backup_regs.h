#pragma once
#include "storage.h"

enum backup_regs_offsets {
    REBOOT_FLAG_OFFSET_BYTE = 0,
};

enum reboot_flags {
    REBOOT_INSTABOOT = 0xB0, /* 176 - immediately (re)boot the orb */
};

static inline int
backup_clear_reboot_flag(void)
{
    int ret = backup_regs_write_byte(REBOOT_FLAG_OFFSET_BYTE, 0);
    return ret;
}
