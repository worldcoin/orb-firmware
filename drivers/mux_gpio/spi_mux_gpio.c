/*
 * Copyright (c) 2023 Tools for Humanity GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT tfh_spi_mux_gpio

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(spi_mux_gpio, CONFIG_SPI_LOG_LEVEL);

struct spi_mux_gpio_root_config {
    const struct device *spi_device;
    const struct gpio_dt_spec enable_gpio;
    const uint8_t number_of_mux_gpios;
    const struct gpio_dt_spec mux_gpios[];
};

struct spi_mux_gpio_root_data {
    struct k_mutex lock;
    uint8_t selected_chan;
};

struct spi_mux_gpio_channel_config {
    const struct device *root;
    uint8_t chan_addr;
};

static inline struct spi_mux_gpio_root_data *
get_root_data_from_channel(const struct device *dev)
{
    const struct spi_mux_gpio_channel_config *channel_config = dev->config;

    return channel_config->root->data;
}

static inline const struct spi_mux_gpio_root_config *
get_root_config_from_channel(const struct device *dev)
{
    const struct spi_mux_gpio_channel_config *channel_config = dev->config;

    return channel_config->root->config;
}

static int
spi_mux_gpio_set_channel(const struct device *dev, uint8_t channel_addr)
{
    int res = 0;
    struct spi_mux_gpio_root_data *data = dev->data;
    const struct spi_mux_gpio_root_config *root_config = dev->config;

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
spi_mux_gpio_transceive(const struct device *dev,
                        const struct spi_config *config,
                        const struct spi_buf_set *tx_bufs,
                        const struct spi_buf_set *rx_bufs)
{
    struct spi_mux_gpio_root_data *data = get_root_data_from_channel(dev);
    const struct spi_mux_gpio_root_config *root_config =
        get_root_config_from_channel(dev);
    const struct spi_mux_gpio_channel_config *down_cfg = dev->config;
    int res;

    res = k_mutex_lock(&data->lock, K_MSEC(5000));
    if (res != 0) {
        return res;
    }

    res = spi_mux_gpio_set_channel(down_cfg->root, down_cfg->chan_addr);
    if (res != 0) {
        goto exit;
    }

    res = spi_transceive(root_config->spi_device, config, tx_bufs, rx_bufs);

    if (root_config->enable_gpio.port != NULL &&
        gpio_pin_set_dt(&root_config->enable_gpio, 0)) {
        LOG_ERR("failed to reset 'enable' pin");
        res = -EIO;
    }

exit:
    k_mutex_unlock(&data->lock);
    return res;
}

__unused static int
spi_mux_gpio_transceive_async(const struct device *dev,
                              const struct spi_config *config,
                              const struct spi_buf_set *tx_bufs,
                              const struct spi_buf_set *rx_bufs,
                              spi_callback_t cb, void *userdata)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(config);
    ARG_UNUSED(tx_bufs);
    ARG_UNUSED(rx_bufs);
    ARG_UNUSED(cb);
    ARG_UNUSED(userdata);

    __ASSERT(0, "Not implemented");
    return -1;
}

int
spi_mux_gpio_release(const struct device *dev, const struct spi_config *config)
{
    struct spi_mux_gpio_root_data *data = get_root_data_from_channel(dev);
    const struct spi_mux_gpio_root_config *root_config =
        get_root_config_from_channel(dev);
    int res;

    res = k_mutex_lock(&data->lock, K_MSEC(5000));
    if (res != 0) {
        return res;
    }

    res = spi_release(root_config->spi_device, config);

    k_mutex_unlock(&data->lock);
    return res;
}

static int
spi_mux_gpio_root_init(const struct device *dev)
{
    struct spi_mux_gpio_root_data *spi_mux_gpio = dev->data;
    const struct spi_mux_gpio_root_config *config = dev->config;

    if (!device_is_ready(config->spi_device)) {
        LOG_ERR("SPI bus %s not ready", config->spi_device->name);
        return -ENODEV;
    }

    if (config->enable_gpio.port == NULL ||
        !device_is_ready(config->enable_gpio.port)) {
        LOG_ERR("GPIO port %s not ready", config->enable_gpio.port->name);
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_INACTIVE)) {
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

    spi_mux_gpio->selected_chan = 0;

    LOG_DBG("root init successful for %s", dev->name);

    return 0;
}

static int
spi_mux_gpio_channel_init(const struct device *dev)
{
    const struct spi_mux_gpio_channel_config *chan_cfg = dev->config;

    if (!device_is_ready(chan_cfg->root)) {
        LOG_ERR("SPI mux root %s not ready", chan_cfg->root->name);
        return -ENODEV;
    }

    LOG_DBG("channel init successful for %s", dev->name);

    return 0;
}

static const struct spi_driver_api spi_mux_gpio_api_funcs = {
    .transceive = spi_mux_gpio_transceive,
#ifdef CONFIG_SPI_ASYNC
    .transceive_async = spi_stm32_transceive_async,
#endif
    .release = spi_mux_gpio_release,
};

/**
 *                                  +-----+  +-----+
 *                                  | dev |  | dev |
 * +---------------+                +-----+  +-----+
 * | SoC           |                   |        |
 * |               |          /--------+--------+
 * |   +---------+ |  +------+    child bus @ 0x0, on GPIO values set to 0b00
 * |   | SPI root|-|--| Mux  |
 * |   +---------+ |  +--+---+    child bus @ 0x3, on GPIO values set to 0b11
 * |               |     |    \----------+--------+--------+
 * |     +-------+ |     |               |        |        |
 * |     | GPIOs |-|-----+            +-----+  +-----+  +-----+
 * |     +-------+ |  @ channel       | dev |  | dev |  | dev |
 * +---------------+                  +-----+  +-----+  +-----+
 */
#define SPI_MUX_GPIO_CHILD_DEFINE(node_id)                                     \
    static const struct spi_mux_gpio_channel_config                            \
        spi_mux_gpio_down_config_##node_id = {                                 \
            .chan_addr = DT_REG_ADDR(node_id),                                 \
            .root = DEVICE_DT_GET(DT_PARENT(node_id)),                         \
    };                                                                         \
    BUILD_ASSERT(                                                              \
        DT_REG_ADDR(node_id) <                                                 \
            (1 << DT_PROP_LEN(DT_PARENT(node_id), mux_gpios)),                 \
        "Address (reg) cannot be used with the specified number of IOs");      \
    DEVICE_DT_DEFINE(node_id, spi_mux_gpio_channel_init, NULL, NULL,           \
                     &spi_mux_gpio_down_config_##node_id, POST_KERNEL,         \
                     CONFIG_SPI_MUX_GPIO_CHANNEL_INIT_PRIO,                    \
                     &spi_mux_gpio_api_funcs);

#define SPI_MUX_GPIO_ROOT_DEFINE(inst)                                         \
    static const struct spi_mux_gpio_root_config spi_mux_gpio_cfg__##inst = {  \
        .spi_device = DEVICE_DT_GET(DT_INST_PHANDLE(inst, spi_parent)),        \
        .enable_gpio = GPIO_DT_SPEC_GET_OR(DT_INST(inst, tfh_spi_mux_gpio),    \
                                           enable_gpios, {0}),                 \
        .number_of_mux_gpios =                                                 \
            DT_PROP_LEN(DT_INST(inst, tfh_spi_mux_gpio), mux_gpios),           \
        .mux_gpios = {DT_FOREACH_PROP_ELEM_SEP(                                \
            DT_DRV_INST(inst), mux_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))}};    \
    static struct spi_mux_gpio_root_data spi_mux_gpio_data_##inst = {          \
        .lock = Z_MUTEX_INITIALIZER(spi_mux_gpio_data_##inst.lock),            \
    };                                                                         \
    SPI_DEVICE_DT_DEFINE(DT_DRV_INST(inst), spi_mux_gpio_root_init, NULL,      \
                         &spi_mux_gpio_data_##inst, &spi_mux_gpio_cfg__##inst, \
                         POST_KERNEL, CONFIG_SPI_MUX_GPIO_INIT_PRIO, NULL);    \
    DT_FOREACH_CHILD(DT_INST(inst, tfh_spi_mux_gpio),                          \
                     SPI_MUX_GPIO_CHILD_DEFINE);

DT_INST_FOREACH_STATUS_OKAY(SPI_MUX_GPIO_ROOT_DEFINE)

BUILD_ASSERT(CONFIG_SPI_MUX_GPIO_CHANNEL_INIT_PRIO >
                 CONFIG_SPI_MUX_GPIO_INIT_PRIO,
             "Ensure the parent node is initialized before the child nodes");
