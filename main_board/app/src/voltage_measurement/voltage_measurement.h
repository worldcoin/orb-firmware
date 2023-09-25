#pragma once

#include "errors.h"
#include "mcu_messaging.pb.h"
#include <app_assert.h>
#include <stm32_ll_adc.h>
#include <zephyr/devicetree.h>

/**
 * Sorted list of all possible voltage monitoring channels as defined in the
 * DTS file.
 */
typedef enum {
    CHANNEL_VBAT_SW = 0,
    CHANNEL_PVCC,
    CHANNEL_12V,
    CHANNEL_12V_CAPS,
    CHANNEL_DIE_TEMP,
    CHANNEL_3V3_UC,
    CHANNEL_1V8,
    CHANNEL_3V3,
    CHANNEL_5V,
    CHANNEL_3V3_SSD_3V8, // 3V3_SSD on EV5; 3V8 on EV1...4
    CHANNEL_VREFINT,
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    CHANNEL_3V3_LTE,
#endif
    CHANNEL_COUNT
} voltage_measurement_channel_t;

static inline uint16_t
voltage_measurement_get_vref_mv_from_raw(Hardware_OrbVersion hardware_version,
                                         uint16_t vrefint_raw)
{
    return (uint16_t)(hardware_version ==
                              Hardware_OrbVersion_HW_VERSION_PEARL_EV5
                          ? DT_PROP_OR(DT_PATH(zephyr_user), ev5_vref_mv, 0)
                          : __LL_ADC_CALC_VREFANALOG_VOLTAGE(
                                vrefint_raw == 0 ? 1 : vrefint_raw,
                                LL_ADC_RESOLUTION_12B));
}

/**
 * @brief Initialize the voltage measurement module.
 *
 * @param *hw_version Mainboard hardware version
 *
 * @return error code
 * @retval RET_SUCCESS on success
 */
ret_code_t
voltage_measurement_init(const Hardware *hw_version);

/**
 * @brief Gets the measured voltage of a specific channel.
 *
 * @param channel Voltage measurement channel
 * @param *voltage_mv Measured voltage in millivolts.
 *
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_INVALID_PARAM if channel values is not valid.
 * @retval RET_ERROR_NOT_INITIALIZED if voltage_measurement_init() was not
 *      called successfully before using this function.
 */
ret_code_t
voltage_measurement_get(voltage_measurement_channel_t channel,
                        int32_t *voltage_mv);

/**
 * @brief Gets the adc raw value of a specific channel.
 *
 * @param channel Voltage measurement channel
 * @param *adc_raw_value 12 bit ADC raw value
 *
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_INVALID_PARAM if channel values is not valid.
 * @retval RET_ERROR_NOT_INITIALIZED if voltage_measurement_init() was not
 *      called successfully before using this function.
 */
ret_code_t
voltage_measurement_get_raw(voltage_measurement_channel_t channel,
                            uint16_t *adc_raw_value);

/**
 * @brief Gets the voltage at the VREF+ pin.
 *
 * @return VREF+ voltage in mV.
 */
uint16_t
voltage_measurement_get_vref_mv(void);

/**
 * @brief Sets the publishing period for sending all measured voltages to the
 *      Jetson.
 *
 * @param publish_period_ms Publishing period in milliseconds. If 0 then
 *      all voltages will be published only once and publishing will be
 *      disabled after that.
 */
void
voltage_measurement_set_publish_period(uint32_t publish_period_ms);
