#ifndef ORB_MCU_MAIN_APP_RGB_LEDS_H
#define ORB_MCU_MAIN_APP_RGB_LEDS_H

#include <compilers.h>

#define ARRAY_SIZE_ASSERT(arr)                                                 \
    (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define RGB_LED_ORANGE                                                         \
    {                                                                          \
        255, 255 / 2, 0                                                        \
    }

#define RGB_LEDS_OFF(leds) memset(leds, 0, sizeof leds)

#define RGB_LEDS_WHITE(leds, brightness) memset(leds, brightness, sizeof leds)

#define RGB_LEDS_RED(leds, brightness)                                         \
    for (size_t i = 0; i < ARRAY_SIZE_ASSERT(leds); ++i) {                     \
        leds[i].r = brightness;                                                \
        leds[i].g = 0;                                                         \
        leds[i].b = 0;                                                         \
    }

#define RGB_LEDS_GREEN(leds, brightness)                                       \
    for (size_t i = 0; i < ARRAY_SIZE_ASSERT(leds); ++i) {                     \
        leds[i].r = 0;                                                         \
        leds[i].g = brightness;                                                \
        leds[i].b = 0;                                                         \
    }

#define RGB_LEDS_BLUE(leds, brightness)                                        \
    for (size_t i = 0; i < ARRAY_SIZE_ASSERT(leds); ++i) {                     \
        leds[i].r = 0;                                                         \
        leds[i].g = 0;                                                         \
        leds[i].b = brightness;                                                \
    }

#endif // ORB_MCU_MAIN_APP_RGB_LEDS_H
