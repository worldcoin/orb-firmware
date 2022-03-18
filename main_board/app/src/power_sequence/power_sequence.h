#ifndef POWER_SEQUENCE_H
#define POWER_SEQUENCE_H

#include "stdint.h"

int
power_turn_on_super_cap_charger(void);
int
power_turn_on_pvcc(void);
int
power_turn_on_jetson(void);
int
power_reset(uint32_t delay_s);
void
power_reboot_set_pending(void);
void
power_reboot_clear_pending(void);

#endif // POWER_SEQUENCE_H
