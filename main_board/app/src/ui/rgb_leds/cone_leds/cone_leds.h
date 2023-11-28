#pragma once

#include "mcu_messaging.pb.h"
#include <errors.h>

/**
 * Set cone LEDs to a specific color using a sequence of bytes.
 * @param bytes 3-byte long items for each RGB LED
 * @param size size of the array, 3 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 3
 * @retval RET_SUCCESS on success
 */
ret_code_t
cone_leds_set_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size);

/**
 * Set cone LEDs to a specific color using a sequence of bytes.
 * @param bytes 4-byte long items for each ARGB32 LED
 * @param size size of the array, 4 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 4
 * @retval RET_SUCCESS on success
 */
ret_code_t
cone_leds_set_leds_sequence_argb32(const uint8_t *bytes, uint32_t size);

/**
 * Set cone LEDs to a specific color.
 * @param pattern
 * @param color
 * @return
 */
ret_code_t
cone_leds_set_pattern(ConeLEDsPattern_ConeRgbLedPattern pattern,
                      RgbColor *color);

/**
 * Init cone LED thread
 * The LED won't turn on until pattern or brightness is set
 * @return error code:
 * * RET_ERROR_INTERNAL unable to init operator LEDs
 * * RET_SUCCESS successfully initialized
 */
ret_code_t
cone_leds_init(void);
