#include "fan.h"
#include "app_assert.h"
#include "system/version/version.h"
#include <app_config.h>
#include <assert.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "logs_can.h"
LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

#define FAN_MAIN_NODE DT_PATH(fan_main)
#define FAN_AUX_NODE  DT_PATH(fan_aux)

static const struct pwm_dt_spec main_fan_spec = PWM_DT_SPEC_GET(FAN_MAIN_NODE);
static const struct gpio_dt_spec main_fan_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(fan_main), enable_gpios);

#if defined(CONFIG_BOARD_PEARL_MAIN)
static const struct pwm_dt_spec aux_fan_spec = PWM_DT_SPEC_GET(FAN_AUX_NODE);
static const struct gpio_dt_spec aux_fan_enable_spec =
    GPIO_DT_SPEC_GET(DT_PATH(fan_aux), enable_gpios);

BUILD_ASSERT(DEVICE_DT_GET(DT_PWMS_CTLR(FAN_MAIN_NODE)) ==
                 DEVICE_DT_GET(DT_PWMS_CTLR(FAN_AUX_NODE)),
             "We expect the main and aux fan to use the same timer");
#endif

#define MSG "Checking that fan PWM controller is ready... "

struct fan_duty_cycle_specs {
    uint8_t min_duty_cycle_percent; // minimum duty cycle with active fan
    uint8_t max_duty_cycle_percent; // maximum duty cycle with active fan
};

#if defined(CONFIG_BOARD_PEARL_MAIN)
const struct fan_duty_cycle_specs fan_ev1_2_specs = {
    .min_duty_cycle_percent = 0, .max_duty_cycle_percent = 80};
const struct fan_duty_cycle_specs fan_ev3_specs = {
    .min_duty_cycle_percent = 40, .max_duty_cycle_percent = 100};
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
const struct fan_duty_cycle_specs fan_diamond_specs = {
    .min_duty_cycle_percent = 30, .max_duty_cycle_percent = 100};
#endif

static uint16_t fan_speed_by_value = 0; // value over UINT16_MAX range
static struct fan_duty_cycle_specs fan_specs;

uint16_t
fan_get_speed_setting(void)
{
    return fan_speed_by_value;
}

void
fan_set_max_speed(void)
{
    fan_set_speed_by_percentage(100);
}

static uint32_t
compute_pulse_width_ns(uint16_t value)
{
    uint32_t scaled_fan_speed =
        (uint32_t)(value * main_fan_spec.period *
                   ((float)(fan_specs.max_duty_cycle_percent -
                            fan_specs.min_duty_cycle_percent) /
                    100.f)) /
        UINT16_MAX;

    // /!\ multiply first as we don't use floats
    const uint32_t min_period = (uint32_t)(fan_specs.min_duty_cycle_percent *
                                           main_fan_spec.period / 100);
    uint32_t pulse_width_ns = scaled_fan_speed + min_period;

    return pulse_width_ns;
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

    if (value != 0) {
        uint32_t pulse_width_ns = compute_pulse_width_ns(value);

        pwm_set_dt(&main_fan_spec, main_fan_spec.period,
                   (main_fan_spec.period - pulse_width_ns));
#if defined(CONFIG_BOARD_PEARL_MAIN)
        pwm_set_dt(&aux_fan_spec, aux_fan_spec.period,
                   (aux_fan_spec.period - pulse_width_ns));
#endif
    }

    fan_speed_by_value = value;

    // Even at 0%, the fan spins. This will kill power to the fans in the case
    // of 0%.
    if (value > 0) {
        gpio_pin_set_dt(&main_fan_enable_spec, 1);
#if defined(CONFIG_BOARD_PEARL_MAIN)
        gpio_pin_set_dt(&aux_fan_enable_spec, 1);
#endif
    } else {
        gpio_pin_set_dt(&main_fan_enable_spec, 0);
#if defined(CONFIG_BOARD_PEARL_MAIN)
        gpio_pin_set_dt(&aux_fan_enable_spec, 0);
#endif
    }
}

void
fan_set_speed_by_percentage(uint32_t percentage)
{
    fan_set_speed_by_value(UINT16_MAX * (MIN(percentage, 100) / 100.0f));
}

int
fan_init(void)
{
    int ret;

    if (!device_is_ready(main_fan_spec.dev)
#if defined(CONFIG_BOARD_PEARL_MAIN)
        || !device_is_ready(aux_fan_spec.dev)
#endif
    ) {
        LOG_ERR(MSG "no");
        return RET_ERROR_INTERNAL;
    }
    LOG_INF(MSG "yes");

    if (!device_is_ready(main_fan_enable_spec.port)) {
        LOG_ERR("fan_enable pin not ready!");
        return RET_ERROR_INTERNAL;
    }
#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (!device_is_ready(aux_fan_enable_spec.port)) {
        LOG_ERR("fan_enable pin not ready!");
        return RET_ERROR_INTERNAL;
    }
#endif
    ret = gpio_pin_configure_dt(&main_fan_enable_spec, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Error %d: failed to configure %s pin %d for output", ret,
                main_fan_enable_spec.port->name, main_fan_enable_spec.pin);
        return RET_ERROR_INTERNAL;
    }
#if defined(CONFIG_BOARD_PEARL_MAIN)
    ret = gpio_pin_configure_dt(&aux_fan_enable_spec, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Error %d: failed to configure %s pin %d for output", ret,
                aux_fan_enable_spec.port->name, aux_fan_enable_spec.pin);
        return RET_ERROR_INTERNAL;
    }
#endif
    // set specs depending on current hardware
    Hardware_OrbVersion version = version_get_hardware_rev();
#if defined(CONFIG_BOARD_PEARL_MAIN)
    if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV1 ||
        version == Hardware_OrbVersion_HW_VERSION_PEARL_EV2) {
        fan_specs = fan_ev1_2_specs;
    } else if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV3 ||
               version == Hardware_OrbVersion_HW_VERSION_PEARL_EV4 ||
               version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        fan_specs = fan_ev3_specs;
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (version == Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2 ||
        version == Hardware_OrbVersion_HW_VERSION_DIAMOND_B3) {
        fan_specs = fan_diamond_specs;
#endif
    } else {
        LOG_ERR("Not supported main board: %u", version);
    }

#ifdef CONFIG_TEST_FAN
    uint32_t max_speed_pulse_width_ns = 0;
    uint32_t min_speed_pulse_width_ns = 0;
    uint32_t value;
    uint32_t pulse_width_ns;

    if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV1 ||
        version == Hardware_OrbVersion_HW_VERSION_PEARL_EV2) {
        max_speed_pulse_width_ns = 32000;

        // 655 (1% of 65535) *40000 (period) *0.8 (range) / 65535 = 319
        min_speed_pulse_width_ns = 319;
    } else if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV3 ||
               version == Hardware_OrbVersion_HW_VERSION_PEARL_EV4 ||
               version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        max_speed_pulse_width_ns = 40000;

        // min is 40% duty cycle = 0.4*40000
        // + 239 (1% of available range of 60%)
        min_speed_pulse_width_ns = 16239;

    } else if (version == Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2 ||
               version == Hardware_OrbVersion_HW_VERSION_DIAMOND_B3) {
        max_speed_pulse_width_ns = 40000;

        // min is 30% duty cycle = 0.3*40000
        // + 279 (1% of available range of 70%)
        min_speed_pulse_width_ns = 12279;
    }

    fan_set_speed_by_percentage(100);
    value = fan_get_speed_setting();
    pulse_width_ns = compute_pulse_width_ns(value);
    ASSERT_SOFT_BOOL(
        ((int32_t)(pulse_width_ns - max_speed_pulse_width_ns) <= 1) &&
        ((int32_t)(pulse_width_ns - max_speed_pulse_width_ns) >= -1));

    k_msleep(1000);

    fan_set_speed_by_percentage(1);
    value = fan_get_speed_setting();
    pulse_width_ns = compute_pulse_width_ns(value);
    ASSERT_SOFT_BOOL(pulse_width_ns == min_speed_pulse_width_ns);
#endif

    fan_set_speed_by_percentage(FAN_INITIAL_SPEED_PERCENT);

    return RET_SUCCESS;
}
