#ifndef DISTRIBUTOR_LEDS_H
#define DISTRIBUTOR_LEDS_H

#include <mcu_messaging.pb.h>

#define OPERATOR_LEDS_ALL_MASK BIT_MASK(5)

void
operator_leds_set_brightness(uint8_t brightness);

void
operator_leds_set_pattern(
    DistributorLEDsPattern_DistributorRgbLedPattern pattern, uint32_t mask,
    RgbColor *color);

/// Init operator LED thread
/// The LED won't turn on until pattern or brightness is set
/// \return error code:
/// * RET_ERROR_INTERNAL unable to init operator LEDs
/// * RET_SUCCESS successfully initialized
int
operator_leds_init(void);

#endif // DISTRIBUTOR_LEDS_H
