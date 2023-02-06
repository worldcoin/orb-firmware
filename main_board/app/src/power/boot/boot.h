#ifndef POWER_SEQUENCE_H
#define POWER_SEQUENCE_H

#include "stdint.h"

int
boot_turn_on_super_cap_charger(void);

int
boot_turn_on_pvcc(void);

int
boot_turn_on_jetson(void);

int
reboot(uint32_t delay_s);

#endif // POWER_SEQUENCE_H
