#pragma once

#include "stdint.h"

int
boot_turn_on_super_cap_charger(void);

int
boot_turn_on_pvcc(void);

int
boot_turn_on_jetson(void);

int
reboot(uint32_t delay_s);
