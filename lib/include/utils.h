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

#define STATIC_OR_EXTERN static
