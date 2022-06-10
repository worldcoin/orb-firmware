#ifndef ORB_MCU_MAIN_APP_RGB_LEDS_H
#define ORB_MCU_MAIN_APP_RGB_LEDS_H

#include <compilers.h>

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

#endif // ORB_MCU_MAIN_APP_RGB_LEDS_H
