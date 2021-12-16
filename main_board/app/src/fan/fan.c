#include <device.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan);

#include "fan.h"

#define FAN_NODE        DT_PATH(fan)
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
    pwm_pin_set_nsec(fan_pwm, FAN_PWM_CHANNEL, FAN_PWM_PERIOD,
                     (FAN_PWM_PERIOD * percentage) / 100, FAN_PWM_FLAGS);
}

ret_code_t
fan_init(void)
{
    if (!device_is_ready(fan_pwm)) {
        LOG_ERR(MSG " no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG " yes");

    fan_set_speed(25);
    return RET_SUCCESS;
}
