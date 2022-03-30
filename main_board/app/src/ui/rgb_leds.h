//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#ifndef ORB_MCU_MAIN_APP_RGB_LEDS_H
#define ORB_MCU_MAIN_APP_RGB_LEDS_H

#define RGB_LEDS_OFF(leds) memset(leds, 0, sizeof leds)

#define RGB_LEDS_WHITE(leds, brightness) memset(leds, brightness, sizeof leds)

#define RGB_LEDS_RED(leds, brightness)                                         \
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {                               \
        leds[i].r = brightness;                                                \
        leds[i].g = 0;                                                         \
        leds[i].b = 0;                                                         \
    }

#define RGB_LEDS_GREEN(leds, brightness)                                       \
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {                               \
        leds[i].r = 0;                                                         \
        leds[i].g = brightness;                                                \
        leds[i].b = 0;                                                         \
    }

#define RGB_LEDS_BLUE(leds, brightness)                                        \
    for (int i = 0; i < ARRAY_SIZE(leds); ++i) {                               \
        leds[i].r = 0;                                                         \
        leds[i].g = 0;                                                         \
        leds[i].b = brightness;                                                \
    }

#endif // ORB_MCU_MAIN_APP_RGB_LEDS_H
