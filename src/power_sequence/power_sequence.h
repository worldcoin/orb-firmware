#ifndef POWER_SEQUENCE_H
#define POWER_SEQUENCE_H

int
power_turn_on_essential_supplies(void);
int
power_turn_on_super_cap_charger(void);
int
power_turn_on_pvcc(void);
int
power_wait_for_power_button_press(void);
int
power_turn_on_jetson(void);

#endif // POWER_SEQUENCE_H
