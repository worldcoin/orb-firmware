#pragma once

#include "zephyr/sys/util.h"

#ifdef CONFIG_LOG
#include <zephyr/logging/log_ctrl.h>
#endif

#include "zephyr/irq.h"

// Number of elements in member 'field' of structure 'type'
#define STRUCT_MEMBER_ARRAY_SIZE(type, field) ARRAY_SIZE(((type *)0)->field)

// Size(in bytes) of the member 'field' of structure 'type'
#define STRUCT_MEMBER_SIZE_BYTES(type, field) (sizeof(((type *)0)->field))

// Prefer to use macro below to make sure we exit the critical section
// and re-enable interrupts
#define CRITICAL_SECTION_ENTER(k)                                              \
    {                                                                          \
        int k = irq_lock()

#define CRITICAL_SECTION_EXIT(k)                                               \
    irq_unlock(k);                                                             \
    }                                                                          \
    do {                                                                       \
    } while (0) // remove empty statement warning

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_MINIMAL)
// log immediately, i.e., log and wait for messages to flush
#define LOG_INF_IMM(...)                                                       \
    LOG_INF(__VA_ARGS__);                                                      \
    do {                                                                       \
        uint32_t log_buffered_count = log_buffered_cnt();                      \
        while (LOG_PROCESS() && --log_buffered_count)                          \
            ;                                                                  \
    } while (0)

#define LOG_ERR_IMM(...)                                                       \
    LOG_ERR(__VA_ARGS__);                                                      \
    do {                                                                       \
        uint32_t log_buffered_count = log_buffered_cnt();                      \
        while (LOG_PROCESS() && --log_buffered_count)                          \
            ;                                                                  \
    } while (0)
#else
#define LOG_INF_IMM(...)                                                       \
    do {                                                                       \
    } while (0)
#define LOG_ERR_IMM(...)                                                       \
    do {                                                                       \
    } while (0)
#endif

// Limit scope of variable to current file depending on whether we are testing
// or not. When testing, we want to be able to access the variable from other
// files.
#ifdef CONFIG_ZTEST
#define STATIC_OR_EXTERN
#else
#define STATIC_OR_EXTERN static
#endif

// _ANSI_SOURCE is used with newlib to make sure newlib doesn't provide
// primitives conflicting with Zephyr's POSIX definitions which remove
// definition of M_PI, so let's redefine it
#if defined(_ANSI_SOURCE)
// taken from math.h
#define M_PI 3.14159265358979323846
#endif
