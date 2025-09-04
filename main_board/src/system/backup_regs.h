#pragma once

enum backup_regs_offsets {
    REBOOT_FLAG_OFFSET_BYTE = 0,
};

enum reboot_flags {
    REBOOT_INSTABOOT = 0xB0, /* 176 - immediately (re)boot the orb */
};
