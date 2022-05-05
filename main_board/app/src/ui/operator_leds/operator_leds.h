#ifndef DISTRIBUTOR_LEDS_H
#define DISTRIBUTOR_LEDS_H

#include <mcu_messaging.pb.h>

#define OPERATOR_LEDS_COUNT    DT_PROP(DT_NODELABEL(operator_rgb_leds), num_leds)
#define OPERATOR_LEDS_ALL_MASK BIT_MASK(OPERATOR_LEDS_COUNT)

/// Set brightness
/// \param brightness
/// \return
/// * RET_SUCCESS brightness set, will be applied
/// * RET_ERROR_BUSY previous brightness application in progress
int
operator_leds_set_brightness(uint8_t brightness);

/// Set pattern for operator LEDs
/// \param pattern Pattern to apply
/// \param mask Bit mask (max is OPERATOR_LEDS_ALL_MASK)
/// \param color Custom color, NULL to use default
/// \return
/// * RET_SUCCESS pattern set, will be applied
/// * RET_ERROR_BUSY previous pattern application in progress
int
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
