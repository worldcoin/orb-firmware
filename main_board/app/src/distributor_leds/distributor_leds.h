#ifndef DISTRIBUTOR_LEDS_H
#define DISTRIBUTOR_LEDS_H

#include <mcu_messaging.pb.h>

void
distributor_leds_set_brightness(uint8_t brightness);
void
distributor_leds_set_pattern(
    DistributorLEDsPattern_DistributorRgbLedPattern pattern);
void
distributor_leds_off();
int
distributor_leds_init(void);

#endif // DISTRIBUTOR_LEDS_H
