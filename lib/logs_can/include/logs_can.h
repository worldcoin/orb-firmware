#pragma once

#include <zephyr/logging/log.h>

// redirect the logs (only warnings and errors)
// to printk in case we want them to be sent over CAN
// printk will then forward them to the CAN bus (see `logs_init`)
#if defined(CONFIG_ORB_LIB_LOGS_CAN) && !defined(CONFIG_LOG) &&                \
    defined(CONFIG_PRINTK)
#include <zephyr/sys/printk.h>

#undef LOG_WRN
#define LOG_WRN(...)                                                           \
    do {                                                                       \
        printk("<wrn> %s: ", __func__);                                        \
        printk(__VA_ARGS__);                                                   \
        printk("\n");                                                          \
    } while (0)

#undef LOG_ERR
#define LOG_ERR(...)                                                           \
    do {                                                                       \
        printk("<err> %s: ", __func__);                                        \
        printk(__VA_ARGS__);                                                   \
        printk("\n");                                                          \
    } while (0)
#endif

/**
 * @brief Initialize the logging modules
 *
 * This function initializes logging, with either:
 *  - CAN backend used with Zephyr logging
 *  - printk messages redirected to CAN bus
 *
 * @param print Function pointer to wrap and send the log message over CAN
 * @return RET_SUCCESS
 */
int
logs_init(void (*print)(const char *log, size_t size, bool blocking));
