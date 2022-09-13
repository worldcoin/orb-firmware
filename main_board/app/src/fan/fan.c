#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

#include "fan.h"

#define FAN_MAX_SPEED_PERCENTAGE (80)

#define FAN_MAIN_NODE DT_PATH(fan_main)
#define FAN_AUX_NODE  DT_PATH(fan_aux)

static const struct pwm_dt_spec aux_fan_spec = PWM_DT_SPEC_GET(FAN_AUX_NODE);

// Fan enable/disable
static const struct gpio_dt_spec fan_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fans_enable_gpios);

static const struct pwm_dt_spec main_fan_spec = PWM_DT_SPEC_GET(FAN_MAIN_NODE);

static_assert(DEVICE_DT_GET(DT_PWMS_CTLR(FAN_MAIN_NODE)) ==
                  DEVICE_DT_GET(DT_PWMS_CTLR(FAN_AUX_NODE)),
              "We expect the main and aux fan to use the same timer");

#define MSG "Checking that fan PWM controller is ready... "

static uint32_t fan_speed = 0;

uint32_t
fan_get_speed_setting(void)
{
    return fan_speed;
}

void
fan_set_max_speed(void)
{
    fan_set_speed_by_percentage(FAN_MAX_SPEED_PERCENTAGE);
}

// For PWM control, ultimately the timer peripheral uses three (main) registers:
// ARR, CCR, and CNT ARR is a 16-bit value that represents the frequency of the
// PWM signal. CCR is used to set the duty cycle. CNT is the register that is
// continually incremented from 0 and compared against ARR and CCR.
//
// When CNT == ARR, CNT is reset to 0 and the next PWM period begins.
// During each PWM period, the output starts HIGH and only goes low when CNT ==
// CCR.
//
// Therefore CCR <= ARR and the number of distinct duty cycle settings is equal
// to ARR. The amount of time represented by each increment of CNT is determined
// by the clock feeding the timer peripheral and the prescaler of the timer.
//
// So, this function's purpose is to map a 16-bit value into the range 0..ARR,
// inclusive, and assign it to CCR. The maximum value of ARR is 65535, and the
// maximum value of uint16_t is 65535, thus a 16-bit value allows the caller to
// adjust the duty cycle of the fan controller as finely as is possible. In the
// case that ARR < 65535 (which is likely), the 16-bit argument to this function
// will have some values that map to the same CCR value.
void
fan_set_speed_by_value(uint16_t value)
{
    LOG_INF("Switching fan to approximately %.2f%% speed",
            ((float)value / UINT16_MAX) * 100);

    fan_speed = main_fan_spec.period -
                (((float)main_fan_spec.period / UINT16_MAX) * value);

    pwm_set_dt(&main_fan_spec, main_fan_spec.period, fan_speed);

    pwm_set_dt(&aux_fan_spec, aux_fan_spec.period, fan_speed);

    // Even at 0%, the fan spins. This will kill power to the fans in the case
    // of 0%.
    if (value > 0) {
        gpio_pin_set_dt(&fan_enable_spec, 1);
    } else {
        gpio_pin_set_dt(&fan_enable_spec, 0);
    }
}

void
fan_set_speed_by_percentage(uint32_t percentage)
{
    fan_set_speed_by_value(UINT16_MAX * (MIN(percentage, 100) / 100.0f));
}

int
fan_init(const struct device *dev)
{
    ARG_UNUSED(dev);

    if (!device_is_ready(main_fan_spec.dev) ||
        !device_is_ready(aux_fan_spec.dev)) {
        LOG_ERR(MSG "no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG "yes");

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

    fan_set_speed_by_percentage(1);

    return RET_SUCCESS;
}

SYS_INIT(fan_init, POST_KERNEL, SYS_INIT_FAN_INIT_PRIORITY);
