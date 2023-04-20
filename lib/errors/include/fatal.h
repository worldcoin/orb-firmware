#pragma once

#include <stdbool.h>

#define IS_WATCHDOG(reg)                                                       \
    ((reg & RCC_CSR_WWDGRSTF) != 0 || (reg & RCC_CSR_IWDGRSTF) != 0)
#define IS_SOFTWARE(reg)  ((reg & RCC_CSR_SFTRSTF) != 0)
#define IS_PIN(reg)       ((reg & RCC_CSR_PINRSTF) != 0)
#define IS_LOW_POWER(reg) ((reg & RCC_CSR_LPWRRSTF) != 0)
#define IS_BOR(reg)       ((reg & RCC_CSR_BORRSTF) != 0)

/**
 * @brief Get the status register giving the reset reason
 * Use the macros IS_WATCHDOG, IS_SOFTWARE, IS_PIN, IS_LOW_POWER, IS_BOR above
 * to check the reset reason
 *
 * @return status register taken during init
 */
uint32_t
fatal_get_status_register(void);

void
fatal_init(void);
