/*
 * Copyright (c) 2023 Tools for Humanity GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT nxp_sc18is606

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sc18is606, CONFIG_SC18IS606_LOG_LEVEL);

struct sc18is606_root_config {
    const struct i2c_dt_spec i2c_device;
    const struct gpio_dt_spec reset_gpio;
};

struct sc18is606_root_data {
    struct k_mutex lock;
};

struct sc18is606_down_config {
    const struct device *root;
};

static inline struct sc18is606_root_data *
get_root_data_from_channel(const struct device *dev)
{
    const struct sc18is606_down_config *channel_config = dev->config;

    return channel_config->root->data;
}

static inline const struct sc18is606_root_config *
get_root_config_from_channel(const struct device *dev)
{
    const struct sc18is606_down_config *channel_config = dev->config;

    return channel_config->root->config;
}

static int
sc18is606_transceive(const struct device *dev, const struct spi_config *config,
                     const struct spi_buf_set *tx_bufs,
                     const struct spi_buf_set *rx_bufs)
{
    struct sc18is606_root_data *data = get_root_data_from_channel(dev);
    const struct sc18is606_root_config *root_config =
        get_root_config_from_channel(dev);
    int res;
    const uint32_t supported_spi_clock_rates[] = {
        1875000, // F1:F0 = 0
        455000,  // F1:F0 = 1
        115000,  // F1:F0 = 2
        58000    // F1:F0 = 3
    };
    uint8_t sc18is606_spi_config_byte = 0;
    struct i2c_msg spi_chip_select_and_data_msgs[tx_bufs->count + 1];

    if ((tx_bufs == NULL) && (rx_bufs == NULL)) {
        return 0;
    }

    if ((rx_bufs != NULL) && (rx_bufs->count > 0)) {
        LOG_ERR("This driver doesn't support rx operation.");
        return -ENOTSUP;
    }

    if (tx_bufs == NULL) {
        LOG_ERR("No tx data provided.");
        return -EINVAL;
    }

    if (SPI_WORD_SIZE_GET(config->operation) != 8) {
        LOG_ERR("Only 8 bit words supported.");
        return -ENOTSUP;
    }

    bool supported_spi_frequency_configured = false;
    for (size_t i = 0; i < ARRAY_SIZE(supported_spi_clock_rates); i++) {
        if (config->frequency == supported_spi_clock_rates[i]) {
            sc18is606_spi_config_byte |= i;
            supported_spi_frequency_configured = true;
            break;
        }
    }

    if (!supported_spi_frequency_configured) {
        LOG_ERR("SPI frequency of %d Hz not supported.", config->frequency);
        return -EINVAL;
    }

    if (SPI_MODE_GET(config->operation) & SPI_MODE_CPOL) {
        sc18is606_spi_config_byte |= 0x08;
    }

    if (SPI_MODE_GET(config->operation) & SPI_MODE_CPHA) {
        sc18is606_spi_config_byte |= 0x04;
    }

    if (config->operation & SPI_TRANSFER_LSB) {
        // transmit LSB first
        sc18is606_spi_config_byte |= 0x20;
    }

    if (config->operation & SPI_OP_MODE_SLAVE) {
        LOG_ERR("Operation in SPI slave mode is not supported.");
        return -ENOTSUP;
    }

    res = k_mutex_lock(&data->lock, K_MSEC(5000));
    if (res != 0) {
        LOG_ERR("mutex lock failed");
        return res;
    }

    uint8_t i2c_tx_buf[] = {0xF0, sc18is606_spi_config_byte};
    struct i2c_msg spi_config_msg = {
        .buf = i2c_tx_buf,
        .len = sizeof(i2c_tx_buf),
        .flags = I2C_MSG_WRITE,
    };
    res = i2c_transfer_dt(&root_config->i2c_device, &spi_config_msg, 1);
    if (res != 0) {
        LOG_ERR("transfer of SPI config failed");
        goto exit;
    }

    uint8_t chip_select_byte = 0; // for setting SS0, SS1 and SS2 signals -> not
                                  // used in current driver implementation
    spi_chip_select_and_data_msgs[0] = (struct i2c_msg){
        .buf = &chip_select_byte,
        .len = 1,
        .flags = I2C_MSG_WRITE,
    };
    for (int i = 0; i < tx_bufs->count; i++) {
        spi_chip_select_and_data_msgs[i + 1] = (struct i2c_msg){
            .buf = tx_bufs->buffers[i].buf,
            .len = tx_bufs->buffers[i].len,
            .flags = I2C_MSG_WRITE,
        };
    };
    res =
        i2c_transfer_dt(&root_config->i2c_device, spi_chip_select_and_data_msgs,
                        ARRAY_SIZE(spi_chip_select_and_data_msgs));
    if (res != 0) {
        LOG_ERR("transfer of SPI data failed");
        goto exit;
    }

exit:
    k_mutex_unlock(&data->lock);
    return res;
}

__unused static int
sc18is606_transceive_async(const struct device *dev,
                           const struct spi_config *config,
                           const struct spi_buf_set *tx_bufs,
                           const struct spi_buf_set *rx_bufs, spi_callback_t cb,
                           void *userdata)
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
sc18is606_release(const struct device *dev, const struct spi_config *config)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(config);

    // not implemented

    return 0;
}

static int
sc18is606_root_init(const struct device *dev)
{
    const struct sc18is606_root_config *config = dev->config;

    if (!device_is_ready(config->i2c_device.bus)) {
        LOG_ERR("I2C bus %s not ready", config->i2c_device.bus->name);
        return -ENODEV;
    }

    if (config->reset_gpio.port != NULL &&
        gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
        LOG_ERR("Failed to configure %s", dev->name);
        return -EIO;
    }

    LOG_DBG("root init successful for %s", dev->name);

    return 0;
}

static int
sc18is606_channel_init(const struct device *dev)
{
    const struct sc18is606_down_config *chan_cfg = dev->config;

    if (!device_is_ready(chan_cfg->root)) {
        LOG_ERR("I2C mux root %s not ready", chan_cfg->root->name);
        return -ENODEV;
    }

    return 0;
}

static const struct spi_driver_api sc18is606_api_funcs = {
    .transceive = sc18is606_transceive,
#ifdef CONFIG_SPI_ASYNC
    .transceive_async = sc18is606_transceive_async,
#endif
    .release = sc18is606_release,
};

#define SC18IS606_CHILD_DEFINE(node_id)                                        \
    static const struct sc18is606_down_config                                  \
        sc18is606_down_config_##node_id = {                                    \
            .root = DEVICE_DT_GET(DT_PARENT(node_id)),                         \
    };                                                                         \
    BUILD_ASSERT(DT_REG_ADDR(node_id) == 0, "Address (reg) must be 0");        \
    DEVICE_DT_DEFINE(node_id, sc18is606_channel_init, NULL, NULL,              \
                     &sc18is606_down_config_##node_id, POST_KERNEL,            \
                     CONFIG_SC18IS606_CHANNEL_INIT_PRIO,                       \
                     &sc18is606_api_funcs);

#define SC18IS606_ROOT_DEFINE(inst)                                            \
    static const struct sc18is606_root_config sc18is606_cfg__##inst = {        \
        .i2c_device = I2C_DT_SPEC_INST_GET(inst),                              \
        .reset_gpio = GPIO_DT_SPEC_GET_OR(DT_INST(inst, nxp_sc18is606),        \
                                          reset_gpios, {0})};                  \
    static struct sc18is606_root_data sc18is606_data_##inst = {                \
        .lock = Z_MUTEX_INITIALIZER(sc18is606_data_##inst.lock),               \
    };                                                                         \
    I2C_DEVICE_DT_DEFINE(DT_DRV_INST(inst), sc18is606_root_init, NULL,         \
                         &sc18is606_data_##inst, &sc18is606_cfg__##inst,       \
                         POST_KERNEL, CONFIG_SC18IS606_INIT_PRIO, NULL);       \
    DT_FOREACH_CHILD(DT_INST(inst, nxp_sc18is606), SC18IS606_CHILD_DEFINE);

DT_INST_FOREACH_STATUS_OKAY(SC18IS606_ROOT_DEFINE)

BUILD_ASSERT(CONFIG_SC18IS606_CHANNEL_INIT_PRIO > CONFIG_SC18IS606_INIT_PRIO,
             "Ensure the parent node is initialized before the child nodes");
