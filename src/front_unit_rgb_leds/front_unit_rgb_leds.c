#include <app_config.h>
#include <device.h>
#include <drivers/led_strip.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(front_unit_rgb_leds);

#include "front_unit_led_patterns.h"
#include "front_unit_rgb_leds.h"

K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS);
static struct k_thread thread_data;

static void
thread_entry_point(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;

    front_unit_rgb_led_random_loop(led_strip, 20);
}

int
front_unit_rgb_leds_init(void)
{
    const struct device *led_strip = NULL;

#ifdef CONFIG_BOARD_ORB
    led_strip = DEVICE_DT_GET(DT_NODELABEL(front_unit_rgb_leds));
#endif

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return 1;
    }

    k_thread_create(&thread_data, stack_area, K_THREAD_STACK_SIZEOF(stack_area),
                    thread_entry_point, (void *)led_strip, NULL, NULL,
                    THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS, 0, K_NO_WAIT);

    return 0;
}
