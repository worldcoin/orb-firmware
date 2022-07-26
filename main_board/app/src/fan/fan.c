#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan);

#include "fan.h"

#define FAN_MAX_SPEED_PERCENTAGE (80)

#ifdef CONFIG_BOARD_MCU_MAIN_V30
#define FAN_MAIN_NODE DT_PATH(fan)
#else
// Main board 3.1
#define FAN_MAIN_NODE DT_PATH(fan_main)
// Aux fan
#define FAN_AUX_NODE  DT_PATH(fan_aux)

static const struct pwm_dt_spec aux_fan_spec = PWM_DT_SPEC_GET(FAN_AUX_NODE);

// Fan enable/disable
static const struct gpio_dt_spec fan_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fans_enable_gpios);
#endif // CONFIG_BOARD_MCU_MAIN_V30

static const struct pwm_dt_spec main_fan_spec = PWM_DT_SPEC_GET(FAN_MAIN_NODE);

#ifdef CONFIG_BOARD_MCU_MAIN_V31
static_assert(DEVICE_DT_GET(DT_PWMS_CTLR(FAN_MAIN_NODE)) ==
                  DEVICE_DT_GET(DT_PWMS_CTLR(FAN_AUX_NODE)),
              "We expect the main and aux fan to use the same timer");
#endif

#define MSG "Checking that fan PWM controller is ready... "

static uint8_t fan_speed = 0;

uint8_t
fan_get_speed(void)
{
    return fan_speed;
}

void
fan_set_max_speed(void)
{
    fan_set_speed(FAN_MAX_SPEED_PERCENTAGE);
}

void
fan_set_speed(uint32_t percentage)
{
    percentage = MIN(percentage, 100);

    fan_speed = percentage;

    LOG_INF("Switching fan to %d%% speed", percentage);

    /* PWM goes to fan directly on 3.0, but it drives an N-channel MOSFET
     * on 3.1, which when activated turns the fan off. So it works in an
     * opposite manner.
     */

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    percentage = 100 - percentage;
#endif

    pwm_set_dt(&main_fan_spec, main_fan_spec.period,
               (main_fan_spec.period * percentage) / 100);

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    pwm_set_dt(&aux_fan_spec, aux_fan_spec.period,
               (aux_fan_spec.period * percentage) / 100);

    // Even at 0%, the fan spins. This will kill power to the fans in the case
    // of 0%.
    if (fan_speed > 0) {
        gpio_pin_set_dt(&fan_enable_spec, 1);
    } else {
        gpio_pin_set_dt(&fan_enable_spec, 0);
    }
#endif
}

int
fan_init(const struct device *dev)
{
    ARG_UNUSED(dev);

    if (!device_is_ready(main_fan_spec.dev)
#ifdef CONFIG_BOARD_MCU_MAIN_V31
        || !device_is_ready(aux_fan_spec.dev)
#endif
    ) {
        LOG_ERR(MSG "no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG "yes");

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    if (!device_is_ready(fan_enable_spec.port)) {
        LOG_ERR("fan_enable pin not ready!");
        return RET_ERROR_INTERNAL;
    }
    int ret = gpio_pin_configure_dt(&fan_enable_spec, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Error %d: failed to configure %s pin %d for output", ret,
                fan_enable_spec.port->name, fan_enable_spec.pin);
        return RET_ERROR_INTERNAL;
    }
#endif

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    fan_set_speed(40);
#else
    fan_set_speed(1);
#endif

    return RET_SUCCESS;
}

SYS_INIT(fan_init, POST_KERNEL, SYS_INIT_FAN_INIT_PRIORITY);
