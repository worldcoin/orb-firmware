//
// Created by Cyril on 26/11/2021.
//
#include "compilers.h"
#include "errors.h"

#ifndef APP_ASSERT_LOG_ERROR
#ifdef CONFIG_LOG_PRINTK
// Zephyr support
#include <kernel.h>
#include <sys/printk.h>
#define APP_ASSERT_LOG_ERROR printk
#define OOPS(x)              k_oops(x)
#else
// Generic printing function
#include <stdio.h>
#define APP_ASSERT_LOG_ERROR printf
#define OOPS(x)
#endif
#endif

static uint32_t error_count = 0;

__WEAK void
app_assert_handler(int32_t error_code, uint32_t line_num,
                   const uint8_t *p_file_name)
{
    APP_ASSERT_LOG_ERROR("Failing assert %s:%d, error %d\n", p_file_name,
                         line_num, error_code);

    ++error_count;

    // stop if debugging
    __BKPT(0);

    // terminate the thread
    OOPS();
}

uint32_t
app_assert_count()
{
    return error_count;
}
