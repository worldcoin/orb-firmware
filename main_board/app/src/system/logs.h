#pragma once

#include <zephyr/logging/log.h>

// release build does not have logging enabled
// redirect the logging to printk which is then
// transmitted over the CAN bus
#if !defined(DEBUG) && defined(CONFIG_PRINTK)
#include <zephyr/sys/printk.h>

#undef LOG_WRN
#define LOG_WRN(...)                                                           \
    do {                                                                       \
        printk("<wrn> %s: ", __func__);                                        \
        printk(__VA_ARGS__);                                                   \
        printk("\n");                                                          \
    } while (0);

//

#undef LOG_ERR
#define LOG_ERR(...)                                                           \
    do {                                                                       \
        printk("<err> %s: ", __func__);                                        \
        printk(__VA_ARGS__);                                                   \
        printk("\n");                                                          \
    } while (0);
#endif

/**
 * @brief Initialize the logging modules
 *
 * This function initializes the logging modules that are used by the
 *     mcu. One custom backend is used: the CAN bus
 *
 * @return RET_SUCCESS
 */
int
logs_init(void);
