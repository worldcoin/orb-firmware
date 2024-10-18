#include <app_config.h>
#include <main.pb.h>
#include <pubsub/pubsub.h>
#include <stm32_ll_rcc.h>
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <system/stm32_timer_utils/stm32_timer_utils.h>
#include <temperature/fan/fan.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "orb_logs.h"
LOG_MODULE_REGISTER(fan_tach, CONFIG_FAN_TACH_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_TEMPERATURE);
static struct k_thread thread_data;

enum isr_state {
    AWAITING_FIRST_SAMPLE,
    AWAITING_SECOND_SAMPLE,
    AWAITING_TIMER_EXPIRATION
};

struct timer_info {
    TIM_TypeDef *timer;
    uint32_t channel;
    uint32_t irq;
    enum isr_state state;
    uint32_t first_cc_value, second_cc_value;
    atomic_t rpm;
};

#define DT_INST_CLK(inst)                                                      \
    {                                                                          \
        .bus = DT_CLOCKS_CELL(DT_PARENT(inst), bus),                           \
        .enr = DT_CLOCKS_CELL(DT_PARENT(inst), bits)                           \
    }

#define ASSUMED_TIMER_CLOCK_FREQ_MHZ 170
#define ASSUMED_TIMER_CLOCK_FREQ     (ASSUMED_TIMER_CLOCK_FREQ_MHZ * 1000000)

/*!
 * Setting the prescaler so that the timer times out after one second
 * allows us to set a minimum measurable RPM of 60RPM (1 full tachometer period
 * in 1 second).
 * @note All the timers are counting using at least a 16-bit register, so the
 * timeout occurs when counter reaches 65535.
 * @note 32-bit timers will in any case be able to capture more than a second
 * before timing out with the CPU rate of 170MHz.
 *
 * Based on a CPU rate of 170MHz, the prescaler must be set to:
 *      (170MHz / 65535) = 2594 = (TACHOMETER_TIMER_PRESCALER + 1)
 */
#define TACHOMETER_TIMER_PRESCALER 2593

#define FAN_MAIN_TACH_NODE DT_NODELABEL(fan_main_tach)
PINCTRL_DT_DEFINE(FAN_MAIN_TACH_NODE);
static struct stm32_pclken fan_main_tach_pclken =
    DT_INST_CLK(FAN_MAIN_TACH_NODE);
BUILD_ASSERT(DT_PROP_LEN(FAN_MAIN_TACH_NODE, channels) == 1,
             "We expect fan main tach to have only 1 channel");
#define FAN_MAIN_TIMER                                                         \
    ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(FAN_MAIN_TACH_NODE)))
#define FAN_MAIN_CHANNEL DT_PROP_BY_IDX(FAN_MAIN_TACH_NODE, channels, 0)
#define FAN_MAIN_IRQn    DT_IRQ_BY_NAME(DT_PARENT(FAN_MAIN_TACH_NODE), global, irq)
static struct timer_info fan_main_timer_info = {
    .timer = FAN_MAIN_TIMER, .channel = FAN_MAIN_CHANNEL, .irq = FAN_MAIN_IRQn};

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define FAN_AUX_TACH_NODE DT_NODELABEL(fan_aux_tach)
PINCTRL_DT_DEFINE(FAN_AUX_TACH_NODE);
static struct stm32_pclken fan_aux_tach_pclken = DT_INST_CLK(FAN_AUX_TACH_NODE);
BUILD_ASSERT(DT_PROP_LEN(FAN_AUX_TACH_NODE, channels) == 1,
             "We expect fan aux tach to have only 1 channel");
#define FAN_AUX_TIMER   ((TIM_TypeDef *)DT_REG_ADDR(DT_PARENT(FAN_AUX_TACH_NODE)))
#define FAN_AUX_CHANNEL DT_PROP_BY_IDX(FAN_AUX_TACH_NODE, channels, 0)
#define FAN_AUX_IRQn    DT_IRQ_BY_NAME(DT_PARENT(FAN_AUX_TACH_NODE), global, irq)
static struct timer_info fan_aux_timer_info = {
    .timer = FAN_AUX_TIMER, .channel = FAN_AUX_CHANNEL, .irq = FAN_AUX_IRQn};
#endif

static struct stm32_pclken *all_pclken[] = {
    &fan_main_tach_pclken,
#if defined(CONFIG_BOARD_PEARL_MAIN)
    &fan_aux_tach_pclken,
#endif
};

static const struct pinctrl_dev_config *pin_controls[] = {
    PINCTRL_DT_DEV_CONFIG_GET(FAN_MAIN_TACH_NODE),
#if defined(CONFIG_BOARD_PEARL_MAIN)
    PINCTRL_DT_DEV_CONFIG_GET(FAN_AUX_TACH_NODE)
#endif
};

#define TIMER_MAX_CH 4

/** Channel to LL mapping. */
static const uint32_t ch2ll[TIMER_MAX_CH] = {
    LL_TIM_CHANNEL_CH1,
    LL_TIM_CHANNEL_CH2,
    LL_TIM_CHANNEL_CH3,
    LL_TIM_CHANNEL_CH4,
};

/** Channel to clear CC flag function mapping. */
static void (*const clear_timer_cc_flag[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_ClearFlag_CC1, LL_TIM_ClearFlag_CC2, LL_TIM_ClearFlag_CC3,
    LL_TIM_ClearFlag_CC4};

/** Channel to enable CC interrupt function mapping. */
static void (*const enable_timer_cc_int[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_EnableIT_CC1,
    LL_TIM_EnableIT_CC2,
    LL_TIM_EnableIT_CC3,
    LL_TIM_EnableIT_CC4,
};

/** Channel to is_active CC int function mapping. */
static uint32_t (*const is_active_timer_cc_int[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_IsActiveFlag_CC1,
    LL_TIM_IsActiveFlag_CC2,
    LL_TIM_IsActiveFlag_CC3,
    LL_TIM_IsActiveFlag_CC4,
};

/** Channel to get CC value function mapping. */
static uint32_t (*const get_timer_cc_value[TIMER_MAX_CH])(TIM_TypeDef *) = {
    LL_TIM_IC_GetCaptureCH1,
    LL_TIM_IC_GetCaptureCH2,
    LL_TIM_IC_GetCaptureCH3,
    LL_TIM_IC_GetCaptureCH4,
};

/** Channel to clear overrun CC function mapping. */
static void (*const clear_overrun_timer_cc_value[TIMER_MAX_CH])(
    TIM_TypeDef *) = {
    LL_TIM_ClearFlag_CC1OVR,
    LL_TIM_ClearFlag_CC2OVR,
    LL_TIM_ClearFlag_CC3OVR,
    LL_TIM_ClearFlag_CC4OVR,
};

/** Channel to is_active overrun CC function mapping. */
static uint32_t (*const is_active_timer_cc_overrun[TIMER_MAX_CH])(
    TIM_TypeDef *) = {
    LL_TIM_IsActiveFlag_CC1OVR,
    LL_TIM_IsActiveFlag_CC2OVR,
    LL_TIM_IsActiveFlag_CC3OVR,
    LL_TIM_IsActiveFlag_CC4OVR,
};

static void
fan_tachometer_isr(void *arg)
{
    struct timer_info *timer_info = (struct timer_info *)arg;

    if (is_active_timer_cc_overrun[timer_info->channel - 1](
            timer_info->timer)) {
        LOG_ERR("Interrupt not serviced fast enough!");
        timer_info->state = AWAITING_FIRST_SAMPLE;
        clear_timer_cc_flag[timer_info->channel - 1](timer_info->timer);
        clear_overrun_timer_cc_value[timer_info->channel - 1](
            timer_info->timer);
        LL_TIM_ClearFlag_UPDATE(timer_info->timer);
    } else if (LL_TIM_IsActiveFlag_UPDATE(timer_info->timer)) {
        if (timer_info->state != AWAITING_TIMER_EXPIRATION) {
            atomic_set(&timer_info->rpm, 0);
        } else {
            uint32_t total =
                timer_info->second_cc_value - timer_info->first_cc_value;
            if (timer_info->first_cc_value >= timer_info->second_cc_value) {
                LOG_ERR("Internal error, second sample came before first");
                atomic_set(&timer_info->rpm, UINT32_MAX);
            } else {
                float hz = ASSUMED_TIMER_CLOCK_FREQ /
                           (float)((TACHOMETER_TIMER_PRESCALER + 1) * total);
                uint32_t rpm = hz * 60;
                atomic_set(&timer_info->rpm, rpm);
            }
            clear_timer_cc_flag[timer_info->channel - 1](timer_info->timer);
            clear_overrun_timer_cc_value[timer_info->channel - 1](
                timer_info->timer);
            LL_TIM_CC_EnableChannel(timer_info->timer,
                                    ch2ll[timer_info->channel - 1]);
        }
        LL_TIM_ClearFlag_UPDATE(timer_info->timer);
        timer_info->state = AWAITING_FIRST_SAMPLE;
    } else if (is_active_timer_cc_int[timer_info->channel - 1](
                   timer_info->timer)) {
        if (timer_info->state == AWAITING_FIRST_SAMPLE) {
            timer_info->first_cc_value =
                get_timer_cc_value[timer_info->channel - 1](timer_info->timer);
            timer_info->state = AWAITING_SECOND_SAMPLE;
        } else if (timer_info->state == AWAITING_SECOND_SAMPLE) {
            timer_info->second_cc_value =
                get_timer_cc_value[timer_info->channel - 1](timer_info->timer);
            timer_info->state = AWAITING_TIMER_EXPIRATION;
            LL_TIM_CC_DisableChannel(timer_info->timer,
                                     ch2ll[timer_info->channel - 1]);
        }
        clear_timer_cc_flag[timer_info->channel - 1](timer_info->timer);
    }
}

uint32_t
fan_tach_get_main_speed(void)
{
    return atomic_get(&fan_main_timer_info.rpm);
}

uint32_t
fan_tach_get_aux_speed(void)
{
#if defined(CONFIG_BOARD_PEARL_MAIN)
    return atomic_get(&fan_aux_timer_info.rpm);
#else
    return 0;
#endif
}

static ret_code_t
config_timer(struct timer_info *timer_info)
{
    LL_TIM_DisableCounter(timer_info->timer);

    LL_TIM_InitTypeDef timer_general_config;
    LL_TIM_StructInit(&timer_general_config);

    timer_general_config.Prescaler = TACHOMETER_TIMER_PRESCALER;
    timer_general_config.CounterMode = LL_TIM_COUNTERMODE_UP;
    timer_general_config.Autoreload = 0xffff;
    timer_general_config.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    timer_general_config.RepetitionCounter = 0;

    if (LL_TIM_Init(timer_info->timer, &timer_general_config)) {
        LOG_ERR("Failed to setup timer!");
        return RET_ERROR_INTERNAL;
    }

    LL_TIM_IC_InitTypeDef timer_input_config;
    LL_TIM_IC_StructInit(&timer_input_config);

    timer_input_config.ICPolarity = LL_TIM_IC_POLARITY_RISING;
    timer_input_config.ICActiveInput = LL_TIM_ACTIVEINPUT_DIRECTTI;
    timer_input_config.ICPrescaler = LL_TIM_ICPSC_DIV2;
    timer_input_config.ICFilter = LL_TIM_IC_FILTER_FDIV1;

    if (LL_TIM_IC_Init(timer_info->timer, ch2ll[timer_info->channel - 1],
                       &timer_input_config)) {
        LOG_ERR("Failed to setup timer as an input channel!");
        return RET_ERROR_INTERNAL;
    }

    clear_timer_cc_flag[timer_info->channel - 1](timer_info->timer);
    LL_TIM_ClearFlag_UPDATE(timer_info->timer);
    LL_TIM_EnableIT_UPDATE(timer_info->timer);
    enable_timer_cc_int[timer_info->channel - 1](timer_info->timer);
    irq_enable(timer_info->irq);
    LL_TIM_EnableCounter(timer_info->timer);

    return RET_SUCCESS;
}

static void
fan_tach_thread()
{
    orb_mcu_main_FanStatus fs;

    while (1) {
        k_msleep(1000);

        uint32_t main_speed = fan_tach_get_main_speed();
        uint32_t aux_speed = fan_tach_get_aux_speed();

        if (main_speed == UINT32_MAX) {
            LOG_ERR("Internal error getting main fan speed!");
        }
        if (aux_speed == UINT32_MAX) {
            LOG_ERR("Internal error getting aux fan speed!");
        }

        LOG_DBG("main fan speed = %" PRIu32 "RPM", main_speed);
        LOG_DBG("aux fan speed = %" PRIu32 "RPM", aux_speed);

        bool speed_sent = false;

        // Only if all fans report a speed of 0 do we send 0

        if (main_speed != 0 && main_speed != UINT32_MAX) {
            fs.measured_speed_rpm = main_speed;
            fs.fan_id = orb_mcu_main_FanStatus_FanID_MAIN;
            publish_new(&fs, sizeof fs, orb_mcu_main_McuToJetson_fan_status_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            speed_sent = true;
        }
        if (aux_speed != 0 && aux_speed != UINT32_MAX) {
            fs.measured_speed_rpm = aux_speed;
            fs.fan_id = orb_mcu_main_FanStatus_FanID_AUX;
            publish_new(&fs, sizeof fs, orb_mcu_main_McuToJetson_fan_status_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            speed_sent = true;
        }

        if (!speed_sent) {
            fs.measured_speed_rpm = 0;
            fs.fan_id = orb_mcu_main_FanStatus_FanID_MAIN;
            publish_new(&fs, sizeof fs, orb_mcu_main_McuToJetson_fan_status_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            fs.fan_id = orb_mcu_main_FanStatus_FanID_AUX;
            publish_new(&fs, sizeof fs, orb_mcu_main_McuToJetson_fan_status_tag,
                        CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }
    }
}

ret_code_t
fan_tach_init(void)
{
    ret_code_t ret;
    k_tid_t thread_id;
    ret = enable_clocks_and_configure_pins(all_pclken, ARRAY_SIZE(all_pclken),
                                           pin_controls,
                                           ARRAY_SIZE(pin_controls));
    if (ret != RET_SUCCESS) {
        return ret;
    }

    IRQ_CONNECT(FAN_MAIN_IRQn, 0, fan_tachometer_isr, &fan_main_timer_info, 0);
#if defined(CONFIG_BOARD_PEARL_MAIN)
    IRQ_CONNECT(FAN_AUX_IRQn, 0, fan_tachometer_isr, &fan_aux_timer_info, 0);
#endif

    ret = config_timer(&fan_main_timer_info);
    if (ret != RET_SUCCESS) {
        return ret;
    }

#if defined(CONFIG_BOARD_PEARL_MAIN)
    ret = config_timer(&fan_aux_timer_info);
    if (ret != RET_SUCCESS) {
        return ret;
    }
#endif

    thread_id = k_thread_create(&thread_data, stack_area,
                                K_THREAD_STACK_SIZEOF(stack_area),
                                fan_tach_thread, NULL, NULL, NULL,
                                THREAD_PRIORITY_FAN_TACH, 0, K_NO_WAIT);
    k_thread_name_set(thread_id, "fan_tach");

    return RET_SUCCESS;
}
