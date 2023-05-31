#pragma once

#include <errors.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>

#define ASSUMED_TIMER_CLOCK_FREQ_MHZ 170
#define ASSUMED_TIMER_CLOCK_FREQ     (ASSUMED_TIMER_CLOCK_FREQ_MHZ * 1000000)

/**
 * Enable timer peripheral clocks and configure the GPIO pins
 * @param periph_clock_enables An array of peripheral clock enable control
 * structs
 * @param periph_clock_enables_len The length of the aforementioned array
 *
 * @param pin_controls An array in pin control structs
 * @param pin_controls_len The length of the aforementioned array
 *
 * @note periph_clock_enables_len and pin_controls_len must be equal
 *
 * @return
 *  * RET_SUCCESS if configuration succeeded
 *  * RET_ERROR_INTERNAL if anything failed
 */
ret_code_t
enable_clocks_and_configure_pins(struct stm32_pclken **periph_clock_enables,
                                 size_t periph_clock_enables_len,
                                 const struct pinctrl_dev_config **pin_controls,
                                 size_t pin_controls_len);

/**
 * Obtain timer clock speed.
 *
 * @param pclken  Timer clock control subsystem.
 * @param tim_clk Where computed timer clock will be stored.
 *
 * @return 0 on success, error code otherwise.
 */
int
get_tim_clk(const struct stm32_pclken *pclken, uint32_t *tim_clk);
