#pragma once

#include "mcu.pb.h"
#include <errors.h>
#include <zephyr/devicetree.h>

#define OPERATOR_LEDS_COUNT    (DT_PROP(DT_NODELABEL(operator_rgb_leds), num_leds))
#define OPERATOR_LEDS_ALL_MASK BIT_MASK(OPERATOR_LEDS_COUNT)

#if OPERATOR_LEDS_COUNT > 1
#define OPERATOR_LEDS_ITERATIONS_COUNT OPERATOR_LEDS_COUNT
#else
#define OPERATOR_LEDS_ITERATIONS_COUNT 12
#endif

/**
 * Set brightness
 * @param brightness
 * @retval RET_SUCCESS brightness set, will be applied
 * @retval RET_ERROR_BUSY previous brightness application in progress
 */
int
operator_leds_set_brightness(uint8_t brightness);

/**
 * Set pattern for operator LEDs
 * @param pattern Pattern to apply
 * @param mask Bit mask (max is OPERATOR_LEDS_ALL_MASK)
 * @param color Custom color, NULL to use default
 * @retval RET_SUCCESS pattern set, will be applied
 * @retval RET_ERROR_BUSY previous pattern application in progress
 */
int
operator_leds_set_pattern(
    orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern pattern,
    uint32_t mask, const orb_mcu_main_RgbColor *color);

/**
 * Init operator LED thread
 * The LED won't turn on until pattern or brightness is set
 * @retval RET_ERROR_INTERNAL unable to init operator LEDs
 * @retval RET_SUCCESS successfully initialized
 */
int
operator_leds_init(void);

/**
 * Set operator LEDs to a specific color using a sequence of bytes.
 * @param bytes 3-byte long items for each RGB LED
 * @param size size of the array, 3 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 3
 * @retval RET_SUCCESS on success
 */
ret_code_t
operator_leds_set_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size);

/**
 * Set operator LEDs to a specific color using a sequence of bytes.
 * @param bytes 4-byte long items for each ARGB32 LED
 * @param size size of the array, 4 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 4
 * @retval RET_SUCCESS on success
 */
ret_code_t
operator_leds_set_leds_sequence_argb32(const uint8_t *bytes, uint32_t size);

/**
 * Set operator LEDs to a specific color using a mask, the LED are
 * actuated before returning.
 * ⚠️ Prefer operator_leds_set_pattern() when possible
 * Set operator LEDs to a specific color using a mask.
 * All communication to the LEDs is done before this function returns.
 * On Diamond: also handles power optimization when all LEDs are off
 * @param mask Bit mask to control LED individually (max is
 * OPERATOR_LEDS_ALL_MASK)
 */
void
operator_leds_set_blocking(const orb_mcu_main_RgbColor *color, uint32_t mask);

/**
 * Show an animation for indicating a low battery
 * @note This function blocks until the animation is finished
 */
void
operator_leds_indicate_low_battery_blocking(void);
