//
// Created by Cyril on 26/11/2021.
//
#include "compilers.h"
#include "errors.h"

#ifndef APP_ASSERT_LOG_ERROR
#ifdef CONFIG_LOG_PRINTK
// Zephyr support
#include <sys/printk.h>
#define APP_ASSERT_LOG_ERROR printk
#else
// Generic printing function
#include <stdio.h>
#define APP_ASSERT_LOG_ERROR printf
#endif
#endif

__WEAK void
app_assert_handler(int32_t error_code, uint32_t line_num,
                   const uint8_t *p_file_name)
{
    APP_ASSERT_LOG_ERROR("Failing assert %s:%d, error %d\n", p_file_name,
                         line_num, error_code);

    // stop if debugging
    __BKPT(0);
}
