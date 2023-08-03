#pragma once

#include <compilers.h>

#define INITIAL_PULSING_PERIOD_MS 5000
#define PULSING_SCALE_DEFAULT     (1.0f)

#define ARRAY_SIZE_ASSERT(arr)                                                 \
    (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

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

#define MINIMUM_WHITE_BRIGHTNESS 9

#define RGB_WHITE                                                              \
    {                                                                          \
        MINIMUM_WHITE_BRIGHTNESS, MINIMUM_WHITE_BRIGHTNESS,                    \
            MINIMUM_WHITE_BRIGHTNESS                                           \
    }

#define RGB_WHITE_OPERATOR_LEDS                                                \
    {                                                                          \
        20, 20, 20                                                             \
    }
