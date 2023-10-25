#pragma once

#include <compilers.h>

#define INITIAL_PULSING_PERIOD_MS 5000
#define PULSING_SCALE_DEFAULT     (1.0f)

#define ARRAY_SIZE_ASSERT(arr)                                                 \
    (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define MINIMUM_WHITE_BRIGHTNESS 9

#if defined(CONFIG_LED_STRIP_RGB_SCRATCH)
#define RGB_ORANGE                                                             \
    {                                                                          \
        .scratch = 0, .r = 255, .g = 255 / 2, .b = 0                           \
    }

#define RGB_ORANGE_LIGHT                                                       \
    {                                                                          \
        .scratch = 0, .r = 4, .g = 2, .b = 0                                   \
    }

#define RGB_OFF                                                                \
    {                                                                          \
        0, 0, 0, 0                                                             \
    }

#define RGB_WHITE                                                              \
    {                                                                          \
        .scratch = 0, .r = MINIMUM_WHITE_BRIGHTNESS,                           \
        .g = MINIMUM_WHITE_BRIGHTNESS, .b = MINIMUM_WHITE_BRIGHTNESS           \
    }

#define RGB_WHITE_OPERATOR_LEDS                                                \
    {                                                                          \
        .scratch = 0, .r = 20, .g = 20, .b = 20                                \
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
