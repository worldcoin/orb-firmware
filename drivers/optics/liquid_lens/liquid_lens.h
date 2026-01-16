/*
 * Copyright (c) 2023 Tools for Humanity GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRIVERS_OPTICS_LIQUID_LENS_H_
#define DRIVERS_OPTICS_LIQUID_LENS_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup liquid_lens_interface Liquid Lens Interface
 * @ingroup io_interfaces
 * @{
 */

#define LIQUID_LENS_MIN_CURRENT_MA (-400)
#define LIQUID_LENS_MAX_CURRENT_MA (400)

/** @brief Liquid lens driver API structure */
struct liquid_lens_driver_api {
    /**
     * @brief Set the target current for the liquid lens
     *
     * @param dev Liquid lens device
     * @param current_ma Target current in milliamps, clamped to
     *                   [LIQUID_LENS_MIN_CURRENT_MA,
     * LIQUID_LENS_MAX_CURRENT_MA]
     * @return 0 on success, negative errno on failure
     */
    int (*set_target_current)(const struct device *dev, int32_t current_ma);

    /**
     * @brief Enable the liquid lens
     *
     * Starts the PWM output and enables the H-bridge gate drivers.
     *
     * @param dev Liquid lens device
     * @return 0 on success, negative errno on failure
     */
    int (*enable)(const struct device *dev);

    /**
     * @brief Disable the liquid lens
     *
     * Stops the PWM output and disables the H-bridge gate drivers.
     *
     * @param dev Liquid lens device
     * @return 0 on success, negative errno on failure
     */
    int (*disable)(const struct device *dev);

    /**
     * @brief Check if the liquid lens is enabled
     *
     * @param dev Liquid lens device
     * @return true if enabled, false otherwise
     */
    bool (*is_enabled)(const struct device *dev);

    /**
     * @brief Configure the current sense parameters
     *
     * Allows runtime configuration of amplifier gain and shunt resistance
     * for different hardware versions.
     *
     * @param dev Liquid lens device
     * @param amplifier_gain Gain of the current sense amplifier
     * @param shunt_resistance_ohms Shunt resistor value in Ohms
     * @return 0 on success, negative errno on failure
     */
    int (*configure_current_sense)(const struct device *dev,
                                   uint32_t amplifier_gain,
                                   float shunt_resistance_ohms);
};

/**
 * @brief Set the target current for the liquid lens
 *
 * @param dev Liquid lens device
 * @param current_ma Target current in milliamps
 * @return 0 on success, negative errno on failure
 */
static inline int
liquid_lens_set_target_current(const struct device *dev, int32_t current_ma)
{
    const struct liquid_lens_driver_api *api =
        (const struct liquid_lens_driver_api *)dev->api;

    if (api->set_target_current == NULL) {
        return -ENOSYS;
    }

    return api->set_target_current(dev, current_ma);
}

/**
 * @brief Enable the liquid lens
 *
 * @param dev Liquid lens device
 * @return 0 on success, negative errno on failure
 */
static inline int
liquid_lens_enable(const struct device *dev)
{
    const struct liquid_lens_driver_api *api =
        (const struct liquid_lens_driver_api *)dev->api;

    if (api->enable == NULL) {
        return -ENOSYS;
    }

    return api->enable(dev);
}

/**
 * @brief Disable the liquid lens
 *
 * @param dev Liquid lens device
 * @return 0 on success, negative errno on failure
 */
static inline int
liquid_lens_disable(const struct device *dev)
{
    const struct liquid_lens_driver_api *api =
        (const struct liquid_lens_driver_api *)dev->api;

    if (api->disable == NULL) {
        return -ENOSYS;
    }

    return api->disable(dev);
}

/**
 * @brief Check if the liquid lens is enabled
 *
 * @param dev Liquid lens device
 * @return true if enabled, false otherwise
 */
static inline bool
liquid_lens_is_enabled(const struct device *dev)
{
    const struct liquid_lens_driver_api *api =
        (const struct liquid_lens_driver_api *)dev->api;

    if (api->is_enabled == NULL) {
        return false;
    }

    return api->is_enabled(dev);
}

/**
 * @brief Configure the current sense parameters
 *
 * Allows runtime configuration of amplifier gain and shunt resistance
 * for different hardware versions.
 *
 * @param dev Liquid lens device
 * @param amplifier_gain Gain of the current sense amplifier
 * @param shunt_resistance_ohms Shunt resistor value in Ohms
 * @return 0 on success, negative errno on failure
 */
static inline int
liquid_lens_configure_current_sense(const struct device *dev,
                                    uint32_t amplifier_gain,
                                    float shunt_resistance_ohms)
{
    const struct liquid_lens_driver_api *api =
        (const struct liquid_lens_driver_api *)dev->api;

    if (api->configure_current_sense == NULL) {
        return -ENOSYS;
    }

    return api->configure_current_sense(dev, amplifier_gain,
                                        shunt_resistance_ohms);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_OPTICS_LIQUID_LENS_H_ */
