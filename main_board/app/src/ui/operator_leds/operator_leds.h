#ifndef DISTRIBUTOR_LEDS_H
#define DISTRIBUTOR_LEDS_H

#include <mcu_messaging.pb.h>

void
operator_leds_set_brightness(uint8_t brightness);

void
operator_leds_set_color(uint8_t red, uint8_t green, uint8_t blue);

#define OPERATOR_LED_SET_ORANGE() operator_leds_set_color(255, 255 / 2, 0)

void
operator_leds_set_pattern(
    DistributorLEDsPattern_DistributorRgbLedPattern pattern);

/// Init operator LED thread
/// The LED won't turn on until pattern or brightness is set
/// \return error code:
/// * RET_ERROR_INTERNAL unable to init operator LEDs
/// * RET_SUCCESS successfully initialized
int
operator_leds_init(void);

#endif // DISTRIBUTOR_LEDS_H
