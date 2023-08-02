#include "button.h"
#include "mcu_messaging.pb.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "system/logs.h"
LOG_MODULE_REGISTER(button, CONFIG_BUTTON_LOG_LEVEL);

#define POWER_BUTTON_NODE DT_PATH(buttons, power_button)

static const struct gpio_dt_spec button_spec =
    GPIO_DT_SPEC_GET_OR(POWER_BUTTON_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
static bool is_init = false;

static void
button_released(struct k_work *item);

static void
button_pressed(struct k_work *item);

static K_WORK_DEFINE(button_pressed_work, button_pressed);
static K_WORK_DEFINE(button_released_work, button_released);

static void
button_released(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    PowerButton button_state = {.pressed = false};
    publish_new(&button_state, sizeof(button_state),
                McuToJetson_power_button_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
button_pressed(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    PowerButton button_state = {.pressed = true};
    publish_new(&button_state, sizeof(button_state),
                McuToJetson_power_button_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

/// Interrupt context
static void
button_event_handler(const struct device *dev, struct gpio_callback *cb,
                     uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(button_spec.pin)) {
        int ret = gpio_pin_get_dt(&button_spec);

        // queue work depending on button state
        if (ret == 1) {
            k_work_submit(&button_pressed_work);
        } else if (ret == 0) {
            k_work_submit(&button_released_work);
        } else {
            // error, do nothing
        }
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

    k_work_init(&button_pressed_work, button_pressed);
    k_work_init(&button_released_work, button_released);

    gpio_init_callback(&button_cb_data, button_event_handler,
                       BIT(button_spec.pin));

    err_code = gpio_add_callback(button_spec.port, &button_cb_data);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    LOG_INF("Power button initialized");
    is_init = true;

    return RET_SUCCESS;
}
