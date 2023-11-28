#pragma once

#include <compilers.h>
#include <errors.h>
#include <stdbool.h>
#include <stdio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

#define INITIAL_PULSING_PERIOD_MS 5000
#define PULSING_SCALE_DEFAULT     (1.0f)

#define ARRAY_SIZE_ASSERT(arr)                                                 \
    (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define MINIMUM_WHITE_BRIGHTNESS 9

#if defined(CONFIG_LED_STRIP_RGB_SCRATCH)

#define RGB_BRIGHTNESS_MAX 31

#define RGB_ORANGE                                                             \
    {                                                                          \
        .scratch = RGB_BRIGHTNESS_MAX, .r = 255, .g = 255 / 2, .b = 0          \
    }

#define RGB_ORANGE_LIGHT                                                       \
    {                                                                          \
        .scratch = RGB_BRIGHTNESS_MAX, .r = 4, .g = 2, .b = 0                  \
    }

#define RGB_OFF                                                                \
    {                                                                          \
        0, 0, 0, 0                                                             \
    }

#define RGB_WHITE                                                              \
    {                                                                          \
        .scratch = RGB_BRIGHTNESS_MAX, .r = MINIMUM_WHITE_BRIGHTNESS,          \
        .g = MINIMUM_WHITE_BRIGHTNESS, .b = MINIMUM_WHITE_BRIGHTNESS           \
    }

#define RGB_WHITE_OPERATOR_LEDS                                                \
    {                                                                          \
        .scratch = RGB_BRIGHTNESS_MAX, .r = 20, .g = 20, .b = 20               \
    }
#else
#define RGB_ORANGE                                                             \
    {                                                                          \
        255, 255 / 2, 0                                                        \
    }

#define RGB_ORANGE_LIGHT                                                       \
    {                                                                          \
        4, 2, 0                                                                \
    }

#define RGB_OFF                                                                \
    {                                                                          \
        0, 0, 0                                                                \
    }

#define RGB_WHITE                                                              \
    {                                                                          \
        MINIMUM_WHITE_BRIGHTNESS, MINIMUM_WHITE_BRIGHTNESS,                    \
            MINIMUM_WHITE_BRIGHTNESS                                           \
    }

#define RGB_WHITE_OPERATOR_LEDS                                                \
    {                                                                          \
        20, 20, 20                                                             \
    }
#endif

#define RGB_WHITE_BUTTON_PRESS                                                 \
    {                                                                          \
        20, 20, 20                                                             \
    }

#define RGB_WHITE_SHUTDOWN                                                     \
    {                                                                          \
        MINIMUM_WHITE_BRIGHTNESS, MINIMUM_WHITE_BRIGHTNESS,                    \
            MINIMUM_WHITE_BRIGHTNESS                                           \
    }

enum led_format {
    LED_FORMAT_RGB = 3,
    LED_FORMAT_ARGB = 4,
};

/**
 * Copy a sequence of colors defined by the input bytes into the led buffer.
 * The LEDs are updated when the update_leds_sem semaphore is given, if
 * any difference is found between the input bytes and the led buffer.
 * The input bytes are expected to be in the format: (A)RGB.
 * @param input_bytes Input bytes to copy the colors from
 * @param input_size_bytes Size of the input, in bytes
 * @param led_format Input format of the LED, which gives the number of bytes
 * per LED
 * @param led_buffer Buffer to copy the colors into
 * @param leds_count Number of LEDs in the led buffer
 * @param use_sequence Boolean to set to true if the sequence should be used
 * @param update_leds_sem Semaphore to signal when the LEDs should be updated
 * @param write_mutex Mutex to lock when writing to the led buffer (optional)
 * @retval RET_ERROR_INVALID_PARAM if the input_size_bytes is not a multiple of
 * bytes_per_led
 * @retval RET_ERROR_FORBIDDEN if bytes_per_led is not 3 or 4
 * @retval RET_SUCCESS on success
 */
ret_code_t
rgb_leds_set_leds_sequence(const uint8_t *input_bytes, size_t input_size_bytes,
                           enum led_format led_format,
                           struct led_rgb *led_buffer, size_t leds_count,
                           bool *use_sequence, struct k_sem *update_leds_sem,
                           struct k_mutex *write_mutex);
