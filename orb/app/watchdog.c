//
// Created by Cyril on 08/07/2021.
//

#include <errors.h>
#include "watchdog.h"
#include "board.h"

#define LSI_FREQUENCY   32000
#define COUNT_LENGTH    12
#define COUNT_MASK      ((1 << COUNT_LENGTH)-1)

static IWDG_HandleTypeDef m_watchdog_handle;

void
watchdog_reload(void)
{
    HAL_IWDG_Refresh(&m_watchdog_handle);
}

void
watchdog_init(uint32_t period_ms)
{
    const int PRESCALER_MAX = 6;
    uint8_t prescale = 0;

    /* Set the count to represent ticks of 8kHz clock (the 32kHz LSI clock
     * divided by 4 = lowest prescaler setting)
     */
    uint32_t count = period_ms << 3;

    /* Prevent underflow */
    if (count == 0)
    {
        count = 1;
    }

    /* Shift count while increasing prescaler as many times as needed to
     * fit into IWDG_RLR
     */
    while ((count - 1) >> COUNT_LENGTH)
    {
        count >>= 1;
        prescale++;
    }

    /* IWDG_RLR actually holds count - 1 */
    count--;

    /* Clamp to max possible period */
    if (prescale > PRESCALER_MAX)
    {
        count = COUNT_MASK;
        prescale = PRESCALER_MAX;
    }

    count &= COUNT_MASK;

    m_watchdog_handle.Instance = IWDG;
    m_watchdog_handle.Init.Prescaler = prescale;
    m_watchdog_handle.Init.Window = count;
    m_watchdog_handle.Init.Reload = count;

    uint32_t err_code = HAL_IWDG_Init(&m_watchdog_handle);
    ASSERT(err_code);
}
