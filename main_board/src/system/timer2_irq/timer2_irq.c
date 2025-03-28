#include "timer2_irq.h"
#include <stm32g474xx.h>
#include <stm32g4xx_ll_tim.h>
#include <zephyr/irq.h>

#define NUM_CHANNELS 4

#define TIMER2_IRQn DT_IRQN(DT_NODELABEL(timers2))

#define TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX 1
#define TIMER2_CHANNEL_3_FAN_TACH_INDEX       2

static bool timer2_isr_enabled = false;

static callback_info_t channel_callbacks[NUM_CHANNELS] = {0};

static const uint32_t CC_IE_FLAGS[4] = {TIM_DIER_CC1IE, TIM_DIER_CC2IE,
                                        TIM_DIER_CC3IE, TIM_DIER_CC4IE};

static const uint32_t CC_IF_FLAGS[4] = {TIM_SR_CC1IF, TIM_SR_CC2IF,
                                        TIM_SR_CC3IF, TIM_SR_CC4IF};

static void
timer2_isr(const void *arg)
{
    ARG_UNUSED(arg);
    TIM_TypeDef *timer = TIM2;

    // Each of the timer channels are used with different interrupts, so they
    // are serviced individually

    if ((timer->DIER & CC_IE_FLAGS[TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX]) &&
        (timer->SR & CC_IF_FLAGS[TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX])) {
        if (channel_callbacks[TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX].callback) {
            channel_callbacks[TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX].callback(
                channel_callbacks[TIMER2_CHANNEL_2_POLARIZER_STEP_INDEX]
                    .context);
        }
        timer->SR &= ~CC_IF_FLAGS[1];
    }

    if ((timer->DIER & CC_IE_FLAGS[TIMER2_CHANNEL_3_FAN_TACH_INDEX]) &&
        ((timer->SR & CC_IF_FLAGS[TIMER2_CHANNEL_3_FAN_TACH_INDEX]) ||
         (timer->SR & TIM_SR_UIF))) {
        if (channel_callbacks[TIMER2_CHANNEL_3_FAN_TACH_INDEX].callback) {
            channel_callbacks[TIMER2_CHANNEL_3_FAN_TACH_INDEX].callback(
                channel_callbacks[TIMER2_CHANNEL_3_FAN_TACH_INDEX].context);
        }
    }
}

void
timer2_init(void)
{
    IRQ_CONNECT(TIMER2_IRQn, 0, timer2_isr, NULL, 0);
}

void
timer2_register_callback(uint8_t channel, void (*callback)(void *context),
                         void *context)
{
    if (callback != NULL) {
        if (channel >= 1 && channel <= NUM_CHANNELS) {
            channel_callbacks[channel - 1].callback = callback;
            channel_callbacks[channel - 1].context = context;
        }
    }
}

void
timer2_disable_isr(void)
{
    irq_disable(TIMER2_IRQn);
    timer2_isr_enabled = false;
}

void
timer2_enable_isr(void)
{
    irq_enable(TIMER2_IRQn);
    timer2_isr_enabled = true;
}
