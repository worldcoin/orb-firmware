#include "button.h"
#include "mcu_messaging.pb.h"
#include "power_sequence/power_sequence.h"
#include "ui/front_leds/front_leds.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <device.h>
#include <drivers/gpio.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(button);

#define POWER_BUTTON_NODE DT_PATH(buttons, power_button)

static const struct gpio_dt_spec button_spec =
    GPIO_DT_SPEC_GET_OR(POWER_BUTTON_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
static bool is_init = false;
static struct k_work_delayable dwork_detect_shutdown;
#define BUTTON_PRESS_SHUTDOWN_DURATION_MS 5000

/// Interrupt context
static void
button_event_handler(const struct device *dev, struct gpio_callback *cb,
                     uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(button_spec.pin)) {
        int ret = gpio_pin_get_dt(&button_spec);
        if (ret >= 0) {
            McuMessage button_state = {
                .which_message = McuMessage_m_message_tag,
                .message.m_message.which_payload = McuToJetson_power_button_tag,
                .message.m_message.payload.power_button.pressed = ret};
            can_messaging_async_tx(&button_state);

            if (ret == 1) {
                // button pressed
                ret =
                    k_work_schedule(&dwork_detect_shutdown,
                                    K_MSEC(BUTTON_PRESS_SHUTDOWN_DURATION_MS));
                if (ret >= 0) {
                    return;
                }
            } else if (ret == 0) {
                // button released, cancel scheduled work
                k_work_cancel_delayable(&dwork_detect_shutdown);
            }
        }
    }
}

static void
button_press_delayed_worker(struct k_work *work)
{
    UNUSED_PARAMETER(work);

    // 5 second press detected, init shutdown sequence
    if (gpio_pin_get_dt(&button_spec) == 1) {
        LOG_INF("Button pressed %us, shutting down",
                BUTTON_PRESS_SHUTDOWN_DURATION_MS);
        power_reset(1);
    }
}

int
button_uninit(void)
{
    if (!is_init) {
        return RET_ERROR_INVALID_STATE;
    }

    int ret = gpio_pin_interrupt_configure_dt(&button_spec, GPIO_INT_DISABLE);
    if (ret) {
        LOG_ERR("Error disabling button interrupt");
        return ret;
    }

    ret = gpio_remove_callback(button_spec.port, &button_cb_data);
    if (ret) {
        LOG_ERR("Error removing button interrupt");
        return ret;
    }

    is_init = false;

    return ret;
}

int
button_init(void)
{
    int err_code = 0;

    if (is_init) {
        return RET_SUCCESS;
    }

    if (!device_is_ready(button_spec.port)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INVALID_STATE;
    }

    // configure, using devicetree flags + GPIO_INPUT
    err_code = gpio_pin_configure_dt(&button_spec, GPIO_INPUT);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    // configure interrupt
    err_code =
        gpio_pin_interrupt_configure_dt(&button_spec, GPIO_INT_EDGE_BOTH);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&button_cb_data, button_event_handler,
                       BIT(button_spec.pin));

    err_code = gpio_add_callback(button_spec.port, &button_cb_data);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    k_work_init_delayable(&dwork_detect_shutdown, button_press_delayed_worker);

    LOG_INF("Power button initialized");
    is_init = true;

    return RET_SUCCESS;
}
