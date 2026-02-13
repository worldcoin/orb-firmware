/*
 * Copyright (c) 2025 Tools for Humanity Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tfh_lm51xx

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lm51xx, CONFIG_REGULATOR_LOG_LEVEL);

struct lm51xx_config {
    struct regulator_common_config common;
    struct gpio_dt_spec enable;
    struct gpio_dt_spec mode;
};

struct lm51xx_data {
    struct regulator_common_data common;
};

static int
lm51xx_enable(const struct device *dev)
{
    const struct lm51xx_config *cfg = dev->config;

    return gpio_pin_set_dt(&cfg->enable, 1);
}

static int
lm51xx_disable(const struct device *dev)
{
    const struct lm51xx_config *cfg = dev->config;

    return gpio_pin_set_dt(&cfg->enable, 0);
}

static DEVICE_API(regulator, lm51xx_api) = {
    .enable = lm51xx_enable,
    .disable = lm51xx_disable,
};

static int
lm51xx_init(const struct device *dev)
{
    const struct lm51xx_config *cfg = dev->config;
    int ret;

    regulator_common_data_init(dev);

    if (!gpio_is_ready_dt(&cfg->enable)) {
        LOG_ERR("GPIO port: %s not ready", cfg->enable.port->name);
        return -ENODEV;
    }

    /* Use GPIO_OUTPUT_INACTIVE so that PCA95xx expanders (whose output
     * register defaults to all-1s) do not briefly enable the converter
     * at boot.
     */
    ret = gpio_pin_configure_dt(&cfg->enable, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    if (cfg->mode.port != NULL) {
        if (!gpio_is_ready_dt(&cfg->mode)) {
            LOG_ERR("GPIO port: %s not ready", cfg->mode.port->name);
            return -ENODEV;
        }

        ret = gpio_pin_configure_dt(&cfg->mode, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            return ret;
        }
    }

    return regulator_common_init(dev, false);
}

#define LM51XX_DEFINE(inst)                                                    \
    static struct lm51xx_data data##inst;                                      \
                                                                               \
    static const struct lm51xx_config config##inst = {                         \
        .common = REGULATOR_DT_INST_COMMON_CONFIG_INIT(inst),                  \
        .enable = GPIO_DT_SPEC_INST_GET(inst, enable_gpios),                   \
        .mode = GPIO_DT_SPEC_INST_GET_OR(inst, mode_gpios, {0}),               \
    };                                                                         \
                                                                               \
    DEVICE_DT_INST_DEFINE(inst, lm51xx_init, NULL, &data##inst, &config##inst, \
                          POST_KERNEL, CONFIG_LM51XX_REGULATOR_INIT_PRIORITY,  \
                          &lm51xx_api);

DT_INST_FOREACH_STATUS_OKAY(LM51XX_DEFINE)
