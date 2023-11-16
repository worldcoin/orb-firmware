#pragma once

#include <errors.h>
#include <stdint.h>

ret_code_t
white_leds_set_brightness(uint32_t brightness_thousandth);

ret_code_t
white_leds_init(void);
