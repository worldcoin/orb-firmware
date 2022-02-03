#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan);

#include "fan.h"

#ifdef CONFIG_BOARD_MCU_MAIN_V30
#define FAN_NODE DT_PATH(fan)
#else
#define FAN_NODE DT_PATH(fan_main)
#endif

#define FAN_PWM_CTLR    DT_PWMS_CTLR(FAN_NODE)
#define FAN_PWM_CHANNEL DT_PWMS_CHANNEL(FAN_NODE)
#define FAN_PWM_PERIOD  DT_PWMS_PERIOD(FAN_NODE)
#define FAN_PWM_FLAGS   DT_PWMS_FLAGS(FAN_NODE)

#define MSG "Checking that fan PWM controller is ready... "

const struct device *fan_pwm = DEVICE_DT_GET(FAN_PWM_CTLR);

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

    pwm_pin_set_nsec(fan_pwm, FAN_PWM_CHANNEL, FAN_PWM_PERIOD,
                     (FAN_PWM_PERIOD * percentage) / 100, FAN_PWM_FLAGS);
}

int
fan_init(const struct device *dev)
{
    ARG_UNUSED(dev);

    if (!device_is_ready(fan_pwm)) {
        LOG_ERR(MSG "no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG "yes");

    fan_set_speed(0);
    return RET_SUCCESS;
}

static_assert(FAN_INIT_PRIORITY == 54, "update the integer literal here");

// Init this before we init the power supplies, so that
// the fan is not blasting at 100%, which is the hardware
// default.
// We must use an integer literal. Thanks C macros!

SYS_INIT(fan_init, POST_KERNEL, 54);
