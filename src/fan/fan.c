#include <zephyr.h>
#include <device.h>
#include <drivers/pwm.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan);

#include "fan.h"

#define FAN_NODE DT_PATH(fan)
#define FAN_PWM_CTLR DT_PWMS_CTLR(FAN_NODE)
#define FAN_PWM_CHANNEL DT_PWMS_CHANNEL(FAN_NODE)
#define FAN_PWM_PERIOD DT_PWMS_PERIOD(FAN_NODE)
#define FAN_PWM_FLAGS DT_PWMS_FLAGS(FAN_NODE)

#define MSG "Checking that fan PWM controller is ready... "

int turn_on_fan(void)
{
	const struct device *fan_pwm = DEVICE_DT_GET(FAN_PWM_CTLR);
	if (!device_is_ready(fan_pwm)) {
		LOG_ERR(MSG " no");
		return 1;
	}
	LOG_INF(MSG " yes");
	LOG_INF("Switching fan to 100%% speed");
	return pwm_pin_set_nsec(fan_pwm, FAN_PWM_CHANNEL, FAN_PWM_PERIOD, FAN_PWM_PERIOD, FAN_PWM_FLAGS);
}
