#include "errors.h"
#include "fan.h"
#include "fan_tach.h"
#include "orb_state.h"
#include <app_assert.h>
#include <app_config.h>
#include <main.pb.h>
#include <pubsub/pubsub.h>
#include <utils.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(fan_tach, CONFIG_FAN_TACH_LOG_LEVEL);

ORB_STATE_MODULE_DECLARE(fan_tach);

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_FAN_TACH);
static struct k_thread thread_data;

static const struct gpio_dt_spec pwm_tach_gpio =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), fan_main_tach_gpios);
static struct gpio_callback pwm_gpio_callback;

static uint64_t capture_ms = 0;
static size_t rising_edge_counter = 0;
static uint64_t fan_speed_rpm = 0;

uint32_t
fan_tach_get_main_speed(void)
{
    return fan_speed_rpm;
}

static void
fan_tach_thread()
{
    orb_mcu_main_FanStatus fs;

    while (true) {
        k_msleep(1000);

        if (capture_ms != 0 && rising_edge_counter != 0) {
            // per bub0812hn datasheet:
            // fan running for 4 poles <=> 4 edges per revolution <=> 2 rising
            // edges
            CRITICAL_SECTION_ENTER(k);
            const double period =
                (double)capture_ms / (double)rising_edge_counter * 2.0
                /* 2 rising edges per revolution */;
            capture_ms = 0;
            rising_edge_counter = 0;
            fan_speed_rpm = (uint32_t)(1000.0 * 60.0 / period);
            CRITICAL_SECTION_EXIT(k);
        } else {
            fan_speed_rpm = 0;
        }

        LOG_INF("%llu rpm", fan_speed_rpm);

        fs.measured_speed_rpm = fan_speed_rpm;
        fs.fan_id = orb_mcu_main_FanStatus_FanID_MAIN;
        (void)publish_new(&fs, sizeof fs,
                          orb_mcu_main_McuToJetson_fan_status_tag,
                          CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX);
    }
}

static void
fan_tach_event_handler(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins)
{
    UNUSED(dev);
    UNUSED(cb);

    static uint64_t last_capture = 0;
    if (pins & BIT(pwm_tach_gpio.pin)) {
        if (last_capture == 0) {
            last_capture = k_uptime_ticks();
            return; // first capture, ignore
        }

        const int64_t tick = k_uptime_ticks();
        capture_ms += k_ticks_to_ms_floor64(tick - last_capture);
        rising_edge_counter++;
        last_capture = tick;
    }
}

ret_code_t
fan_tach_init(void)
{
    int err_code;
    k_thread_create(&thread_data, stack_area, K_THREAD_STACK_SIZEOF(stack_area),
                    (k_thread_entry_t)fan_tach_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY_FAN_TACH, 0, K_NO_WAIT);
    k_thread_name_set(&thread_data, "fan_tach");

    // set up GPIO interrupt
    if (!gpio_is_ready_dt(&pwm_tach_gpio)) {
        LOG_ERR("Fan tach GPIO not ready");
        err_code = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    err_code = gpio_pin_configure_dt(&pwm_tach_gpio, GPIO_INPUT);
    if (err_code) {
        ASSERT_SOFT(err_code);
        err_code = RET_ERROR_INTERNAL;
        goto exit;
    }

    // configure interrupt
    err_code =
        gpio_pin_interrupt_configure_dt(&pwm_tach_gpio, GPIO_INT_EDGE_RISING);
    if (err_code) {
        ASSERT_SOFT(err_code);
        err_code = RET_ERROR_INTERNAL;
        goto exit;
    }

    gpio_init_callback(&pwm_gpio_callback, fan_tach_event_handler,
                       BIT(pwm_tach_gpio.pin));

    err_code = gpio_add_callback(pwm_tach_gpio.port, &pwm_gpio_callback);
    if (err_code) {
        ASSERT_SOFT(err_code);
        err_code = RET_ERROR_INTERNAL;
        goto exit;
    }

exit:
    if (err_code != RET_SUCCESS) {
        ORB_STATE_SET_CURRENT(RET_ERROR_INTERNAL, "init failed");
    } else {
        ORB_STATE_SET_CURRENT(RET_SUCCESS);
    }

    return RET_SUCCESS;
}
