#pragma once

#include "mcu.pb.h"
#include <errors.h>

#define FULL_RING_DEGREES (360)

/**
 * @brief Initialize the front LEDs
 * @return error code
 */
ret_code_t
front_leds_init(void);

/**
 * Check the LED array for any non-zero LED in the shroud
 * If the semaphore is not available, shroud is assumed to be on.
 *
 * @return true if at least one led is on in the shroud
 */
bool
front_leds_is_shroud_on(void);

/**
 * @brief Set pattern for front LEDs
 * Some arguments are optional because they are not used by some patterns.
 * Make sure to check the pattern documentation.
 *
 * @note When @arg pulsing_scale is used: the @arg color is multiplied by
 *       (1 + @arg pulsing_scale). The result cannot overflow an @c uint8_t.
 *
 * @param pattern pattern to use
 * @param start_angle (optional) start angle in degrees [0 - 360]
 * @param angle_length (optional) angle length in degrees [0 - 360]
 * @param color (optional) color to use
 * @param pulsing_period_ms (optional) period of pulsing in milliseconds
 * @param pulsing_scale (optional) scale of pulsing.
 * @retval RET_ERROR_INVALID_PARAM if some arguments are invalid with the
 * pattern
 * @retval RET_SUCCESS on success
 */
ret_code_t
front_leds_set_pattern(orb_mcu_main_UserLEDsPattern_UserRgbLedPattern pattern,
                       uint32_t start_angle, int32_t angle_length,
                       orb_mcu_main_RgbColor *color, uint32_t pulsing_period_ms,
                       float pulsing_scale);

/**
 * Set center front LEDs to a specific color using a sequence of bytes.
 * @param bytes 3-byte long items for each RGB LED
 * @param size size of the array, 3 * number of LEDs
 * @return
 */
ret_code_t
front_leds_set_center_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size);

/**
 * Set center front LEDs to a specific color using a sequence of bytes.
 * @param bytes 4-byte long items for each ARGB32 LED
 * @param size size of the array, 4 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 4
 * @retval RET_SUCCESS on success
 */
ret_code_t
front_leds_set_center_leds_sequence_argb32(const uint8_t *bytes, uint32_t size);

/**
 * Set ring LEDs to a specific color using a sequence of bytes.
 * @param bytes 3-byte long items for each RGB LED
 * @param size size of the array, 3 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 3
 * @retval RET_SUCCESS on success
 */
ret_code_t
front_leds_set_ring_leds_sequence_rgb24(const uint8_t *bytes, uint32_t size);

/**
 * Set ring LEDs to a specific color using a sequence of bytes.
 * @param bytes 4-byte long items for each ARGB32 LED
 * @param size size of the array, 4 * number of LEDs
 * @retval RET_ERROR_INVALID_PARAM if size is not a multiple of 4
 * @retval RET_SUCCESS on success
 */
ret_code_t
front_leds_set_ring_leds_sequence_argb32(const uint8_t *bytes, uint32_t size);

/**
 * Set front LED brightness, both center and ring
 * Brightness is used with a subset of patterns
 * like:
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_WHITE
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_NO_CENTER
 *  - UserLEDsPattern_UserRgbLedPattern_RANDOM_RAINBOW
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_RED
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_GREEN
 *  - UserLEDsPattern_UserRgbLedPattern_ALL_BLUE
 * @param brightness level [0 - 255]
 */
void
front_leds_set_brightness(uint32_t brightness);

/**
 * Turn off front LEDs
 * LEDs are turned off when this function returns
 */
void
front_leds_turn_off_blocking(void);

/** Perform front led self-test
 * by actuating the LEDs and looking at data & clock test signals that circling
 * back from the led strip. The test lines should go back to normal after
 * inactivity.
 */
int
front_leds_self_test(void);

enum boot_progress_step_e {
    BOOT_PROGRESS_STEP_UNKNOWN = 0,

    /* boot events come in that order: */
    BOOT_PROGRESS_STEP_JETSON_BOOT = 1,
    BOOT_PROGRESS_STEP_OPTICS_INITIALIZED = 2,
    BOOT_PROGRESS_STEP_JETSON_VALUEGET = 3,
    BOOT_PROGRESS_STEP_DATE_SET = 4,

    BOOT_PROGRESS_SENTINEL, // = 5-step boot
    BOOT_PROGRESS_STEP_DONE = BOOT_PROGRESS_SENTINEL,
};

/**
 * @brief Set the boot progress step for the boot animation
 *
 * Updates the boot animation progress. Progress can only increase and
 * each step can only be set once (discard with returning RET_SUCCESS)
 *
 * @param step Boot progress step (BOOT_PROGRESS_STEP_JETSON_BOOT to
 *             BOOT_PROGRESS_STEP_DONE)
 * @return RET_SUCCESS on success
 * @return RET_ERROR_INVALID_PARAM if step is out of range
 */
int
front_leds_boot_progress_set(enum boot_progress_step_e step);

#if defined(CONFIG_BOARD_PEARL_MAIN)
/**
 * Notifies that the IR leds are now off within their duty cycle
 * This allows synchronization of the RGB LEDs update to prevents flickering
 * on Pearl Orbs.
 */
void
front_leds_notify_ir_leds_off(void);
#endif
