/*
 * Copyright (c) 2022 Intel Corporation
 * Copyright (c) 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "icm40627.h"
#include "icm40627_i2c.h"
#include "icm40627_reg.h"
#include "icm40627_trigger.h"

LOG_MODULE_DECLARE(ICM40627, CONFIG_SENSOR_LOG_LEVEL);

static void
icm40627_gpio_callback(const struct device *dev, struct gpio_callback *cb,
                       uint32_t pins)
{
    struct icm40627_dev_data *data =
        CONTAINER_OF(cb, struct icm40627_dev_data, gpio_cb);

    ARG_UNUSED(dev);
    ARG_UNUSED(pins);

#if defined(CONFIG_ICM40627_TRIGGER_OWN_THREAD)
    k_sem_give(&data->gpio_sem);
#elif defined(CONFIG_ICM40627_TRIGGER_GLOBAL_THREAD)
    k_work_submit(&data->work);
#endif
}

static void
icm40627_thread_cb(const struct device *dev)
{
    struct icm40627_dev_data *data = dev->data;

    icm40627_lock(dev);

    if (data->data_ready_handler != NULL) {
        data->data_ready_handler(dev, data->data_ready_trigger);
    }

    icm40627_unlock(dev);
}

#if defined(CONFIG_ICM40627_TRIGGER_OWN_THREAD)

static void
icm40627_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct icm40627_dev_data *data = p1;

    while (1) {
        k_sem_take(&data->gpio_sem, K_FOREVER);
        icm40627_thread_cb(data->dev);
    }
}

#elif defined(CONFIG_ICM40627_TRIGGER_GLOBAL_THREAD)

static void
icm40627_work_handler(struct k_work *work)
{
    struct icm40627_dev_data *data =
        CONTAINER_OF(work, struct icm40627_dev_data, work);

    icm40627_thread_cb(data->dev);
}

#endif

int
icm40627_trigger_set(const struct device *dev,
                     const struct sensor_trigger *trig,
                     sensor_trigger_handler_t handler)
{
    struct icm40627_dev_data *data = dev->data;
    const struct icm40627_dev_cfg *cfg = dev->config;
    uint8_t status;
    int res = 0;

    if (trig == NULL || handler == NULL) {
        return -EINVAL;
    }

    icm40627_lock(dev);
    gpio_pin_interrupt_configure_dt(&cfg->gpio_int1, GPIO_INT_DISABLE);

    switch (trig->type) {
    case SENSOR_TRIG_DATA_READY:
        data->data_ready_handler = handler;
        data->data_ready_trigger = trig;

        icm40627_lock(dev);
        icm40627_i2c_read(&cfg->i2c, REG_INT_STATUS, &status, 1);
        icm40627_unlock(dev);
        break;
    default:
        res = -ENOTSUP;
        break;
    }

    icm40627_unlock(dev);
    gpio_pin_interrupt_configure_dt(&cfg->gpio_int1, GPIO_INT_EDGE_TO_ACTIVE);

    return res;
}

int
icm40627_trigger_init(const struct device *dev)
{
    struct icm40627_dev_data *data = dev->data;
    const struct icm40627_dev_cfg *cfg = dev->config;
    int res = 0;

    if (!cfg->gpio_int1.port) {
        LOG_ERR("trigger enabled but no interrupt gpio supplied");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&cfg->gpio_int1)) {
        LOG_ERR("gpio_int1 not ready");
        return -ENODEV;
    }

    data->dev = dev;
    gpio_pin_configure_dt(&cfg->gpio_int1, GPIO_INPUT);
    gpio_init_callback(&data->gpio_cb, icm40627_gpio_callback,
                       BIT(cfg->gpio_int1.pin));
    res = gpio_add_callback(cfg->gpio_int1.port, &data->gpio_cb);

    if (res < 0) {
        LOG_ERR("Failed to set gpio callback");
        return res;
    }

    k_mutex_init(&data->mutex);
#if defined(CONFIG_ICM40627_TRIGGER_OWN_THREAD)
    k_sem_init(&data->gpio_sem, 0, K_SEM_MAX_LIMIT);
    k_thread_create(&data->thread, data->thread_stack,
                    CONFIG_ICM40627_THREAD_STACK_SIZE, icm40627_thread, data,
                    NULL, NULL, K_PRIO_COOP(CONFIG_ICM40627_THREAD_PRIORITY), 0,
                    K_NO_WAIT);
#elif defined(CONFIG_ICM40627_TRIGGER_GLOBAL_THREAD)
    data->work.handler = icm40627_work_handler;
#endif
    return gpio_pin_interrupt_configure_dt(&cfg->gpio_int1,
                                           GPIO_INT_EDGE_TO_ACTIVE);
}

int
icm40627_trigger_enable_interrupt(const struct device *dev)
{
    int res;
    const struct icm40627_dev_cfg *cfg = dev->config;

    /* pulse-mode (auto clearing), push-pull and active-high */
    res = icm40627_i2c_single_write(&cfg->i2c, REG_INT_CONFIG,
                                    BIT_INT1_DRIVE_CIRCUIT | BIT_INT1_POLARITY);
    if (res != 0) {
        return res;
    }

    /* Deassert async reset for proper INT pin operation, see datasheet 14.50 */
    res = icm40627_i2c_single_write(&cfg->i2c, REG_INT_CONFIG1, 0);
    if (res != 0) {
        return res;
    }

    /* enable data ready interrupt on INT1 pin */
    return icm40627_i2c_single_write(&cfg->i2c, REG_INT_SOURCE0,
                                     FIELD_PREP(BIT_UI_DRDY_INT1_EN, 1));
}

void
icm40627_lock(const struct device *dev)
{
    struct icm40627_dev_data *data = dev->data;

    k_mutex_lock(&data->mutex, K_FOREVER);
}

void
icm40627_unlock(const struct device *dev)
{
    struct icm40627_dev_data *data = dev->data;

    k_mutex_unlock(&data->mutex);
}
