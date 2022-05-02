#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan);

#include "fan.h"

#ifdef CONFIG_BOARD_MCU_MAIN_V30
#define FAN_MAIN_NODE DT_PATH(fan)
#else
// Main board 3.1
#define FAN_MAIN_NODE       DT_PATH(fan_main)
// Aux fan
#define FAN_AUX_NODE        DT_PATH(fan_aux)
#define FAN_AUX_PWM_CTLR    DT_PWMS_CTLR(FAN_AUX_NODE)
#define FAN_AUX_PWM_CHANNEL DT_PWMS_CHANNEL(FAN_AUX_NODE)
// Fan enable/disable
static const struct gpio_dt_spec fan_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fans_enable_gpios);
#endif // CONFIG_BOARD_MCU_MAIN_V30

#define FAN_MAIN_PWM_CTLR    DT_PWMS_CTLR(FAN_MAIN_NODE)
#define FAN_MAIN_PWM_CHANNEL DT_PWMS_CHANNEL(FAN_MAIN_NODE)
#define FAN_PWM_PERIOD       DT_PWMS_PERIOD(FAN_MAIN_NODE)
#define FAN_PWM_FLAGS        DT_PWMS_FLAGS(FAN_MAIN_NODE)

#define MSG "Checking that fan PWM controller is ready... "

const struct device *fan_pwm = DEVICE_DT_GET(FAN_MAIN_PWM_CTLR);

#ifdef CONFIG_BOARD_MCU_MAIN_V31
static_assert(DEVICE_DT_GET(FAN_MAIN_PWM_CTLR) ==
                  DEVICE_DT_GET(FAN_AUX_PWM_CTLR),
              "We expect the main and aux fan to use the same timer");
#endif

void
fan_set_speed(uint32_t percentage)
{
    if (!device_is_ready(fan_pwm)) {
        return;
    }

    percentage = MIN(percentage, 100);

    LOG_INF("Switching fan to %d%% speed", percentage);

    /* PWM goes to fan directly on 3.0, but it drives an N-channel MOSFET
     * on 3.1, which when activated turns the fan off. So it works in an
     * opposite manner.
     */

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    percentage = 100 - percentage;
#endif

    pwm_pin_set_nsec(fan_pwm, FAN_MAIN_PWM_CHANNEL, FAN_PWM_PERIOD,
                     (FAN_PWM_PERIOD * percentage) / 100, FAN_PWM_FLAGS);

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    pwm_pin_set_nsec(fan_pwm, FAN_AUX_PWM_CHANNEL, FAN_PWM_PERIOD,
                     (FAN_PWM_PERIOD * percentage) / 100, FAN_PWM_FLAGS);

    // Even at 0%, the fan spins. This will kill power to the fans in the case
    // of 0%.
    if (percentage > 0) {
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

    if (!device_is_ready(fan_pwm)
#ifdef CONFIG_BOARD_MCU_MAIN_V31
        || !device_is_ready(fan_enable_spec.port)
#endif
    ) {
        LOG_ERR(MSG "no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG "yes");

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    int ret = gpio_pin_configure_dt(&fan_enable_spec, GPIO_OUTPUT);
    if (!ret) {
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

static_assert(FAN_INIT_PRIORITY == 54, "update the integer literal here");

// Init this before we initialize the power supplies, so that
// the fan is not blasting at 100%, which is the hardware
// default.
// We must use an integer literal. Thanks C macros!

SYS_INIT(fan_init, POST_KERNEL, 54);
