#pragma once

#include <errors.h>
#include <mcu_messaging.pb.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

/**
 * @brief Set the sampling period for the temperature sensors
 *
 * @param sample_period The sampling period in milliseconds, must be between 100
 * and 15000 included
 * @retval RET_SUCCESS The sampling period was set successfully
 * @retval RET_ERROR_INVALID_PARAM @sample_period was not between 100 and 15000
 */
ret_code_t
temperature_set_sampling_period_ms(uint32_t sample_period);

/**
 * @brief Initialize the temperature sensors
 * @param hw_version Initialization depends on the hardware version
 * @param *analog_mux_mutex Mutex for I2C multiplexer which shares control
 * signals with the V_SCAP voltages multiplexer
 */
void
temperature_init(const Hardware *hw_version, struct k_mutex *i2c_mux_mutex);

/**
 * @brief Report a temperature reading to the Jetson
 *
 * This function is public because some temperatures are read by the
 * battery module.
 *
 * @param source The source of the temperature reading
 * @param temperature_in_c The temperature in degrees Celsius
 */
void
temperature_report(Temperature_TemperatureSource source,
                   int32_t temperature_in_c);

/**
 * @brief Check if a temperature is exceeding the operating range
 * @return True if a temperature is exceeding the operating range, false
 * otherwise
 */
bool
temperature_is_in_overtemp(void);
