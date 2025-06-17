#include "white_leds.h"
#include "orb_logs.h"

#include <app_assert.h>
#include <app_config.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(white_leds, CONFIG_WHITE_LEDS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(white_leds_stack_area,
                             THREAD_STACK_SIZE_WHITE_LEDS);
static struct k_thread white_leds_thread_data;
static K_SEM_DEFINE(sem_new_setting, 0, 1);

static const struct pwm_dt_spec white_leds_pwm_spec_dvt =
    PWM_DT_SPEC_GET(DT_PATH(white_leds));
static const struct pwm_dt_spec white_leds_pwm_spec_evt =
    PWM_DT_SPEC_GET(DT_PATH(white_leds_evt));
static const struct pwm_dt_spec *white_leds_pwm_spec = &white_leds_pwm_spec_dvt;

static uint32_t global_brightness_thousandth = 0;

_Noreturn static void
white_leds_thread()
{
    uint32_t brightness_thousandth;

    while (1) {
        k_sem_take(&sem_new_setting, K_FOREVER);

        CRITICAL_SECTION_ENTER(k);
        brightness_thousandth = global_brightness_thousandth;
        CRITICAL_SECTION_EXIT(k);

        int ret = pwm_set_dt(white_leds_pwm_spec, white_leds_pwm_spec->period,
                             white_leds_pwm_spec->period *
                                 brightness_thousandth / 1000UL);
        if (ret) {
            LOG_ERR("Error setting PWM parameters: %d", ret);
        }
    }
}

ret_code_t
white_leds_set_brightness(uint32_t brightness_thousandth)
{
    ret_code_t ret = RET_SUCCESS;

    if (brightness_thousandth > 1000) {
        return RET_ERROR_INVALID_PARAM;
    }

    CRITICAL_SECTION_ENTER(k);
    global_brightness_thousandth = brightness_thousandth;
    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_new_setting);
    return ret;
}

ret_code_t
white_leds_init(const orb_mcu_Hardware *hw_version)
{
    if (hw_version == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (hw_version->version <=
        orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_4) {
        white_leds_pwm_spec = &white_leds_pwm_spec_evt;
    }

    if (!device_is_ready(white_leds_pwm_spec->dev)) {
        const int ret = device_init(white_leds_pwm_spec->dev);
        ASSERT_SOFT(ret);
    }

    if (!device_is_ready(white_leds_pwm_spec->dev)) {
        LOG_ERR("PWM for white LEDs not ready!");
        return RET_ERROR_INTERNAL;
    }

    k_tid_t tid =
        k_thread_create(&white_leds_thread_data, white_leds_stack_area,
                        K_THREAD_STACK_SIZEOF(white_leds_stack_area),
                        (k_thread_entry_t)white_leds_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_WHITE_LEDS, 0, K_NO_WAIT);
    k_thread_name_set(tid, "white_leds");

    return RET_SUCCESS;
}
