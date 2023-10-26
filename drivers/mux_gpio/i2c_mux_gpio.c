/*
 * Copyright (c) 2023 Tools for Humanity GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT tfh_i2c_mux_gpio

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_mux_gpio, CONFIG_I2C_LOG_LEVEL);

struct i2c_mux_gpio_root_config {
    const struct device *i2c_device;
    const struct gpio_dt_spec enable_gpio;
    const uint8_t number_of_mux_gpios;
    const struct gpio_dt_spec mux_gpios[];
};

struct i2c_mux_gpio_root_data {
    struct k_mutex lock;
    uint8_t selected_chan;
};

#if IS_ENABLED(CONFIG_I2C_MUX_GLOBAL_LOCK)
static K_MUTEX_DEFINE(i2c_mux_global_lock);
#endif

struct i2c_mux_gpio_channel_config {
    const struct device *root;
    uint8_t chan_addr;
};

static inline struct i2c_mux_gpio_root_data *
get_root_data_from_channel(const struct device *dev)
{
    const struct i2c_mux_gpio_channel_config *channel_config = dev->config;

    return channel_config->root->data;
}

static inline const struct i2c_mux_gpio_root_config *
get_root_config_from_channel(const struct device *dev)
{
    const struct i2c_mux_gpio_channel_config *channel_config = dev->config;

    return channel_config->root->config;
}

static int
i2c_mux_gpio_configure(const struct device *dev, uint32_t dev_config)
{
    const struct i2c_mux_gpio_root_config *root_config =
        get_root_config_from_channel(dev);

    return i2c_configure(root_config->i2c_device, dev_config);
}

static int
i2c_mux_gpio_set_channel(const struct device *dev, uint8_t channel_addr)
{
    int res = 0;
    struct i2c_mux_gpio_root_data *data = dev->data;
    const struct i2c_mux_gpio_root_config *root_config = dev->config;

    /* Only select the channel if it's different from the last channel */
    if (data->selected_chan != channel_addr) {
        for (size_t i = 0; i < root_config->number_of_mux_gpios; i++) {
            if (gpio_pin_set_dt(&root_config->mux_gpios[i],
                                (channel_addr & BIT(i)) != 0)) {
                LOG_ERR("failed to set channel to %d", channel_addr);
                return -EIO;
            }
        }

        data->selected_chan = channel_addr;
    }

    if (root_config->enable_gpio.port != NULL &&
        gpio_pin_set_dt(&root_config->enable_gpio, 1)) {
        LOG_ERR("failed to set enable gpio");
        return -EIO;
    }

    LOG_DBG("channel set to %d", channel_addr);

    return res;
}

static int
i2c_mux_gpio_transfer(const struct device *dev, struct i2c_msg *msgs,
                      uint8_t num_msgs, uint16_t addr)
{
#if !IS_ENABLED(CONFIG_I2C_MUX_GLOBAL_LOCK)
    struct i2c_mux_gpio_root_data *data = get_root_data_from_channel(dev);
#endif
    const struct i2c_mux_gpio_root_config *root_config =
        get_root_config_from_channel(dev);
    const struct i2c_mux_gpio_channel_config *down_cfg = dev->config;
    int res;

    LOG_DBG("start gpio transfer");

#if IS_ENABLED(CONFIG_I2C_MUX_GLOBAL_LOCK)
    res = k_mutex_lock(&i2c_mux_global_lock, K_MSEC(5000));
#else
    res = k_mutex_lock(&data->lock, K_MSEC(5000));
#endif

    if (res != 0) {
        return res;
    }

    res = i2c_mux_gpio_set_channel(down_cfg->root, down_cfg->chan_addr);
    if (res != 0) {
        goto end_trans;
    }

    res = i2c_transfer(root_config->i2c_device, msgs, num_msgs, addr);

    if (root_config->enable_gpio.port != NULL &&
        gpio_pin_set_dt(&root_config->enable_gpio, 0)) {
        LOG_ERR("failed to reset enable gpio");
        res = -EIO;
    }

end_trans:
#if IS_ENABLED(CONFIG_I2C_MUX_GLOBAL_LOCK)
    k_mutex_unlock(&i2c_mux_global_lock);
#else
    k_mutex_unlock(&data->lock);
#endif
    LOG_DBG("gpio transfer finished");
    return res;
}

static int
i2c_mux_gpio_root_init(const struct device *dev)
{
    struct i2c_mux_gpio_root_data *i2c_i2c_mux_gpio = dev->data;
    const struct i2c_mux_gpio_root_config *config = dev->config;

    if (!device_is_ready(config->i2c_device)) {
        LOG_ERR("I2C bus %s not ready", config->i2c_device->name);
        return -ENODEV;
    }

    if (config->enable_gpio.port != NULL &&
        gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_INACTIVE)) {
        LOG_ERR("Failed to configure %s", dev->name);
        return -EIO;
    }

    for (size_t i = 0; i < config->number_of_mux_gpios; i++) {
        if (gpio_pin_configure_dt(&config->mux_gpios[i],
                                  GPIO_OUTPUT_INACTIVE)) {
            LOG_ERR("Failed to configure %s", dev->name);
            return -EIO;
        }
    }

    i2c_i2c_mux_gpio->selected_chan = 0;

    LOG_DBG("root init successful for %s", dev->name);

    return 0;
}

static int
i2c_mux_gpio_channel_init(const struct device *dev)
{
    const struct i2c_mux_gpio_channel_config *chan_cfg = dev->config;

    if (!device_is_ready(chan_cfg->root)) {
        LOG_ERR("I2C mux root %s not ready", chan_cfg->root->name);
        return -ENODEV;
    }

    LOG_DBG("channel init successful for %s", dev->name);

    return 0;
}

static const struct i2c_driver_api i2c_mux_gpio_api_funcs = {
    .configure = i2c_mux_gpio_configure,
    .transfer = i2c_mux_gpio_transfer,
};

/**
 *                                  +-----+  +-----+
 *                                  | dev |  | dev |
 * +---------------+                +-----+  +-----+
 * | SoC           |                   |        |
 * |               |          /--------+--------+
 * |   +---------+ |  +------+    child bus @ 0x0, on GPIO values set to 0b00
 * |   | I2C root|-|--| Mux  |
 * |   +---------+ |  +--+---+    child bus @ 0x3, on GPIO values set to 0b11
 * |               |     |    \----------+--------+--------+
 * |     +-------+ |     |               |        |        |
 * |     | GPIOs |-|-----+            +-----+  +-----+  +-----+
 * |     +-------+ |  @ channel       | dev |  | dev |  | dev |
 * +---------------+                  +-----+  +-----+  +-----+
 */
#define I2C_MUX_GPIO_CHILD_DEFINE(node_id)                                     \
    static const struct i2c_mux_gpio_channel_config                            \
        i2c_mux_gpio_down_config_##node_id = {                                 \
            .chan_addr = DT_REG_ADDR(node_id),                                 \
            .root = DEVICE_DT_GET(DT_PARENT(node_id)),                         \
    };                                                                         \
    BUILD_ASSERT(                                                              \
        DT_REG_ADDR(node_id) <                                                 \
            (1 << DT_PROP_LEN(DT_PARENT(node_id), mux_gpios)),                 \
        "Address (reg) cannot be used with the specified number of IOs");      \
    DEVICE_DT_DEFINE(node_id, i2c_mux_gpio_channel_init, NULL, NULL,           \
                     &i2c_mux_gpio_down_config_##node_id, POST_KERNEL,         \
                     CONFIG_I2C_MUX_GPIO_CHANNEL_INIT_PRIO,                    \
                     &i2c_mux_gpio_api_funcs);

#define I2C_MUX_GPIO_ROOT_DEFINE(inst)                                         \
    static const struct i2c_mux_gpio_root_config i2c_mux_gpio_cfg_##inst = {   \
        .i2c_device = DEVICE_DT_GET(DT_INST_PHANDLE(inst, i2c_parent)),        \
        .enable_gpio = GPIO_DT_SPEC_GET_OR(DT_INST(inst, tfh_i2c_mux_gpio),    \
                                           enable_gpios, {0}),                 \
        .number_of_mux_gpios =                                                 \
            DT_PROP_LEN(DT_INST(inst, tfh_i2c_mux_gpio), mux_gpios),           \
        .mux_gpios = {DT_FOREACH_PROP_ELEM_SEP(                                \
            DT_DRV_INST(inst), mux_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))}};    \
    static struct i2c_mux_gpio_root_data i2c_mux_gpio_data_##inst = {          \
        .lock = Z_MUTEX_INITIALIZER(i2c_mux_gpio_data_##inst.lock),            \
    };                                                                         \
    I2C_DEVICE_DT_DEFINE(DT_DRV_INST(inst), i2c_mux_gpio_root_init, NULL,      \
                         &i2c_mux_gpio_data_##inst, &i2c_mux_gpio_cfg_##inst,  \
                         POST_KERNEL, CONFIG_I2C_MUX_GPIO_INIT_PRIO, NULL);    \
    DT_FOREACH_CHILD(DT_INST(inst, tfh_i2c_mux_gpio),                          \
                     I2C_MUX_GPIO_CHILD_DEFINE);

DT_INST_FOREACH_STATUS_OKAY(I2C_MUX_GPIO_ROOT_DEFINE)

BUILD_ASSERT(CONFIG_I2C_MUX_GPIO_CHANNEL_INIT_PRIO >
                 CONFIG_I2C_MUX_GPIO_INIT_PRIO,
             "Ensure the parent node is initialized before the child nodes");
