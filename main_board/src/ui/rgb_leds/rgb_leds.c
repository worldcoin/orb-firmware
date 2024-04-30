#include "rgb_leds.h"
#include "errors.h"
#include "logs_can.h"
#include <utils.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(rgb_leds, CONFIG_RGB_LEDS_LOG_LEVEL);

ret_code_t
rgb_leds_set_leds_sequence(const uint8_t *input_bytes, size_t input_size_bytes,
                           enum led_format led_format,
                           struct led_rgb *led_buffer, size_t leds_count,
                           bool *use_sequence, struct k_sem *update_leds_sem,
                           struct k_mutex *write_mutex)
{
    bool found_a_difference = false;
    size_t rgb_offset = led_format == LED_FORMAT_ARGB ? 1 : 0;

    if (led_format != LED_FORMAT_RGB && led_format != LED_FORMAT_ARGB) {
        LOG_ERR("Bytes per LED must be 3 or 4");
        return RET_ERROR_INVALID_PARAM;
    }

    if (input_size_bytes % led_format != 0) {
        LOG_ERR("Bytes must be a multiple of %u", led_format);
        return RET_ERROR_INVALID_PARAM;
    }

    input_size_bytes = MIN(input_size_bytes, leds_count * led_format);

    if (write_mutex != NULL) {
        k_mutex_lock(write_mutex, K_FOREVER);
    }
    CRITICAL_SECTION_ENTER(k);
    for (size_t i = 0; i < (input_size_bytes / led_format); ++i) {
        if (!found_a_difference) {
#if defined(CONFIG_LED_STRIP_RGB_SCRATCH)
            if (led_format == LED_FORMAT_ARGB &&
                led_buffer[i].scratch != input_bytes[i * led_format + 0]) {
                found_a_difference = true;
            }
#endif
            if (led_buffer[i].r !=
                input_bytes[i * led_format + rgb_offset + 0]) {
                found_a_difference = true;
            }
            if (led_buffer[i].g !=
                input_bytes[i * led_format + rgb_offset + 1]) {
                found_a_difference = true;
            }
            if (led_buffer[i].b !=
                input_bytes[i * led_format + rgb_offset + 2]) {
                found_a_difference = true;
            }
        }

#if defined(CONFIG_LED_STRIP_RGB_SCRATCH)
        if (led_format == LED_FORMAT_ARGB) {
            led_buffer[i].scratch =
                input_bytes[i * led_format + 0]; // brightness byte
        } else {
            led_buffer[i].scratch = RGB_BRIGHTNESS_MAX;
        }
#endif
        led_buffer[i].r = input_bytes[i * led_format + rgb_offset];
        led_buffer[i].g = input_bytes[i * led_format + rgb_offset + 1];
        led_buffer[i].b = input_bytes[i * led_format + rgb_offset + 2];
    }

    // turn off all LEDs which are not included in the new sequence
    for (size_t i = input_size_bytes / led_format; i < leds_count; ++i) {
        if (led_buffer[i].r != 0 || led_buffer[i].g != 0 ||
            led_buffer[i].b != 0) {
            found_a_difference = true;
        }
        led_buffer[i] = (struct led_rgb)RGB_OFF;
    }

    *use_sequence = true;

    if (write_mutex != NULL) {
        k_mutex_unlock(write_mutex);
    }
    CRITICAL_SECTION_EXIT(k);

    if (found_a_difference) {
        k_sem_give(update_leds_sem);
    }

    return RET_SUCCESS;
}
