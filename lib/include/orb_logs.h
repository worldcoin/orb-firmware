#pragma once

#include <zephyr/logging/log.h>

// Redirect the logs (warnings and errors)
//
// 1. to Memfault's compact logs if enabled. A binary will have to be
//    uploaded to Memfault's cloud to see the logs.
//  or
// 2. to printk in case we want them to be sent over CAN
//    printk will then forward them to the CAN bus (see `logs_init`)
//  or
// 3. to Zephyr's logging system
#if defined(CONFIG_ORB_LIB_LOGS_CAN) && !defined(CONFIG_ZTEST)

#include "logs_can.h"

#if defined(CONFIG_MEMFAULT) && defined(CONFIG_MEMFAULT_COMPACT_LOG)

#include <memfault/core/debug_log.h>
#include <memfault/core/trace_event.h>

/// Log warnings
#undef LOG_WRN
#define LOG_WRN(...)                                                           \
    do {                                                                       \
        MEMFAULT_LOG_WARN(__VA_ARGS__);                                        \
    } while (0)

/// Each Trace Event will be associated with an Issue.
#undef LOG_ERR
#define LOG_ERR(...)                                                           \
    do {                                                                       \
        MEMFAULT_TRACE_EVENT_WITH_LOG(error, __VA_ARGS__);                     \
    } while (0)

#elif !defined(CONFIG_LOG) && defined(CONFIG_PRINTK)

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
#endif // CONFIG_ORB_LIB_LOGS_CAN
