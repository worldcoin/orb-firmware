#include "stm32_timer_utils.h"
#include "system/logs.h"
LOG_MODULE_REGISTER(stm32_timer_utils);

int
get_tim_clk(const struct stm32_pclken *pclken, uint32_t *tim_clk)
{
    int r;
    const struct device *clk;
    uint32_t bus_clk, apb_psc;

    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    r = clock_control_get_rate(clk, (clock_control_subsys_t *)pclken, &bus_clk);
    if (r < 0) {
        return r;
    }

    if (pclken->bus == STM32_CLOCK_BUS_APB1) {
        apb_psc = STM32_APB1_PRESCALER;
    } else {
        apb_psc = STM32_APB2_PRESCALER;
    }

    /*
     * If the APB prescaler equals 1, the timer clock frequencies
     * are set to the same frequency as that of the APB domain.
     * Otherwise, they are set to twice (×2) the frequency of the
     * APB domain.
     */
    if (apb_psc == 1u) {
        *tim_clk = bus_clk;
    } else {
        *tim_clk = bus_clk * 2u;
    }

    return 0;
}

ret_code_t
enable_clocks_and_configure_pins(struct stm32_pclken **periph_clock_enables,
                                 size_t periph_clock_enables_len,
                                 const struct pinctrl_dev_config **pin_controls,
                                 size_t pin_controls_len)
{
    int r;
    size_t i;
    uint32_t timer_clock_freq;
    const struct device *clk;

    if (periph_clock_enables_len != pin_controls_len) {
        LOG_ERR("Lengths must be equal");
        return RET_ERROR_INTERNAL;
    }

    r = 0;
    clk = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE);

    for (i = 0; i < periph_clock_enables_len; ++i) {
        r = clock_control_on(clk, periph_clock_enables[i]);
        if (r < 0) {
            LOG_ERR("Could not initialize clock (%d)", r);
            return RET_ERROR_INTERNAL;
        }

        r = get_tim_clk(periph_clock_enables[i], &timer_clock_freq);
        if (r < 0) {
            LOG_ERR("Could not obtain timer clock (%d)", r);
            return RET_ERROR_INTERNAL;
        }
        if (timer_clock_freq != ASSUMED_TIMER_CLOCK_FREQ) {
            LOG_ERR("Clock frequency must be " STRINGIFY(
                ASSUMED_TIMER_CLOCK_ASSUMED_TIMER_CLOCK_FREQ));
            return RET_ERROR_INTERNAL;
        }

        r = pinctrl_apply_state(pin_controls[i], PINCTRL_STATE_DEFAULT);

        if (r < 0) {
            LOG_ERR("pinctrl"
                    "setup failed (%d)",
                    r);
            return RET_ERROR_INTERNAL;
        }
    }

    return RET_SUCCESS;
}
