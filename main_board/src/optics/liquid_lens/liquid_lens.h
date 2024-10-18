#pragma once

#include "mcu.pb.h"
#include <errors.h>
#include <stdbool.h>
#include <stdint.h>

#define LIQUID_LENS_MIN_CURRENT_MA (-400)
#define LIQUID_LENS_MAX_CURRENT_MA (400)

/**
 * @brief Sets the target current to maintain
 * @param new_target_current_ma the target current clipped to [-400,400]
 * @retval RET_SUCCESS: All good
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress
 */
ret_code_t
liquid_set_target_current_ma(int32_t new_target_current_ma);

/**
 * @brief Enable the liquid lens
 * @details This will start the timer and ADC sampling
 * It doesn't do anything if the liquid lens is already enabled.
 */
void
liquid_lens_enable(void);

/**
 * @brief Disable the liquid lens
 * @details This will stop the timer and ADC sampling.
 */
void
liquid_lens_disable(void);

/**
 * @brief Check if the liquid lens is enabled
 * @details by checking if the ADC device is enabled
 * @return true if liquid lens is enabled, false otherwise
 */
bool
liquid_lens_is_enabled(void);

/**
 * @brief Initialize the liquid lens
 * @details This will initialize the ADC and DMA-controlled clocks
 * used to control the liquid lens
 * @param *hw_version Mainboard hardware version
 * @return error code
 */
ret_code_t
liquid_lens_init(const orb_mcu_Hardware *hw_version);
