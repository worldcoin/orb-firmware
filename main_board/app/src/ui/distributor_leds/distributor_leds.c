#include <app_config.h>
#include <device.h>
#include <drivers/led_strip.h>
#include <drivers/pwm.h>
#include <errors.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(distributor_leds);

#include "distributor_leds.h"
#include "ui/rgb_leds.h"

static K_THREAD_STACK_DEFINE(distributor_leds_stack_area,
                             THREAD_STACK_SIZE_DISTRIBUTOR_RGB_LEDS);
static struct k_thread distributor_leds_thread_data;
static K_SEM_DEFINE(sem, 1, 1); // init to 1 to use default values below

#define NUM_LEDS DT_PROP(DT_NODELABEL(distributor_rgb_leds), num_leds)
static struct led_rgb leds[NUM_LEDS];

// default values
static volatile DistributorLEDsPattern_DistributorRgbLedPattern global_pattern =
    DistributorLEDsPattern_DistributorRgbLedPattern_ALL_WHITE;
static uint8_t global_intensity = 20;

_Noreturn static void
distributor_leds_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct device *led_strip = a;

    for (;;) {
        k_sem_take(&sem, K_FOREVER);

        switch (global_pattern) {
        case DistributorLEDsPattern_DistributorRgbLedPattern_OFF:
            RGB_LEDS_OFF(leds);
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_WHITE:
            RGB_LEDS_WHITE(leds, global_intensity);
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_RED:
            RGB_LEDS_RED(leds, global_intensity);
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_GREEN:
            RGB_LEDS_GREEN(leds, global_intensity);
            break;
        case DistributorLEDsPattern_DistributorRgbLedPattern_ALL_BLUE:
            RGB_LEDS_BLUE(leds, global_intensity);
            break;
        }

        led_strip_update_rgb(led_strip, leds, ARRAY_SIZE(leds));
    }
}

void
distributor_leds_set_brightness(uint8_t brightness)
{
    global_intensity = brightness;
    k_sem_give(&sem);
}

void
distributor_leds_set_pattern(
    DistributorLEDsPattern_DistributorRgbLedPattern pattern)
{
    global_pattern = pattern;
    k_sem_give(&sem);
}

int
distributor_leds_init(void)
{
    const struct device *led_strip =
        DEVICE_DT_GET(DT_NODELABEL(distributor_rgb_leds));

    if (!device_is_ready(led_strip)) {
        LOG_ERR("Front unit LED strip not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid = k_thread_create(
        &distributor_leds_thread_data, distributor_leds_stack_area,
        K_THREAD_STACK_SIZEOF(distributor_leds_stack_area),
        distributor_leds_thread, (void *)led_strip, NULL, NULL,
        THREAD_PRIORITY_DISTRIBUTOR_RGB_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "Distributor RGB LED");

    return RET_SUCCESS;
}
