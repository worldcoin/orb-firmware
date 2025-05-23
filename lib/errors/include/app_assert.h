#pragma once

#include "compilers.h"
#include "stdint.h"

#if CONFIG_MEMFAULT
#include "memfault/panics/assert.h"
#endif

typedef struct {
    char filename[128];
    uint32_t line_num;
    int32_t err_code;
} fatal_error_info_t;

uint32_t
app_assert_soft_count();

void
app_assert_init(void (*assert_callback)(fatal_error_info_t *err_info));

#ifdef CONFIG_ORB_LIB_ERRORS_APP_ASSERT

/**
 * Assert handler for fatal errors.
 * This function makes the system reset in thread mode and schedule error
 * handling in handler mode
 * @param error_code
 * @param line_num
 * @param p_file_name
 */
void
app_assert_hard_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name);

/**
 * Assert handler for recoverable errors. This function returns.
 * @param error_code
 * @param line_num
 * @param p_file_name
 * @param opt_message optional message
 */
void
app_assert_soft_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name, const uint8_t *opt_message);

/**@brief Macro for calling error handler function if supplied error code any
 * other than 0.
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define ASSERT_SOFT(ERR_CODE)                                                  \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            app_assert_soft_handler((ERR_CODE), __LINE__, (uint8_t *)__FILE__, \
                                    NULL);                                     \
        }                                                                      \
    } while (0)

#define ASSERT_SOFT_WITH_MSG(ERR_CODE, MESSAGE)                                \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            app_assert_soft_handler((ERR_CODE), __LINE__,                      \
                                    (const uint8_t *)__FILE__,                 \
                                    (const uint8_t *)MESSAGE);                 \
        }                                                                      \
    } while (0)

/**@brief Macro for calling error handler function if supplied error code any
 * other than 0.
 * Won't return if error, see app_assert_hard_handler
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#if CONFIG_MEMFAULT
#define ASSERT_HARD(ERR_CODE)                                                  \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            MEMFAULT_ASSERT_RECORD(ERR_CODE);                                  \
        }                                                                      \
    } while (0)

#else
#define ASSERT_HARD(ERR_CODE)                                                  \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            app_assert_hard_handler((ERR_CODE), __LINE__,                      \
                                    (uint8_t *)__FILE__);                      \
        }                                                                      \
    } while (0)
#endif

/**@brief Macro for calling error handler function if supplied boolean value is
 * false.
 * Won't return if error
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#ifdef CONFIG_MEMFAULT
#define ASSERT_HARD_BOOL(ERR_CODE) MEMFAULT_ASSERT(ERR_CODE)
#else
#define ASSERT_HARD_BOOL(BOOLEAN_VALUE)                                        \
    do {                                                                       \
        if (!(BOOLEAN_VALUE)) {                                                \
            app_assert_hard_handler(0, __LINE__, (uint8_t *)__FILE__);         \
        }                                                                      \
    } while (0)
#endif

/**@brief Macro for calling error handler function if supplied boolean value is
 * false.
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#define ASSERT_SOFT_BOOL(BOOLEAN_VALUE)                                        \
    do {                                                                       \
        if (!(BOOLEAN_VALUE)) {                                                \
            app_assert_soft_handler(0, __LINE__, (uint8_t *)__FILE__, NULL);   \
        }                                                                      \
    } while (0)

#else
#define ASSERT_SOFT(ERR_CODE)      UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_HARD(ERR_CODE)      UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_HARD_BOOL(ERR_CODE) UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_SOFT_BOOL(ERR_CODE) UNUSED_VARIABLE(ERR_CODE)
#endif

#if defined(CONFIG_ORB_LIB_ERRORS) && !defined(BUILD_FROM_CI)

/// Based on fatal and assert tests, see
/// zephyr/tests/ztest/error_hook/README.txt

/* test case type */
enum error_case_e {
    FATAL_RANDOM = 0, // random fatal error among the following:
    FATAL_ACCESS,
    FATAL_ILLEGAL_INSTRUCTION,
    FATAL_BUSFAULT,
    FATAL_MEMMANAGE,
    FATAL_DIVIDE_ZERO,
    FATAL_K_PANIC,
    FATAL_K_OOPS,
    FATAL_IN_ISR,
    ASSERT_FAIL,
    USER_ASSERT_HARD,
#if defined(CONFIG_IRQ_OFFLOAD)
    ASSERT_IN_ISR,
    USER_ASSERT_HARD_IN_ISR,
#endif
#if defined(CONFIG_USERSPACE)
    USER_FATAL_Z_OOPS,
#endif
#if defined(CONFIG_WATCHDOG)
    FATAL_WATCHDOG,
#endif
    TEST_ERROR_CASE_COUNT
};

/**
 * Test a fatal error condition
 * Does not return: make sure the microcontroller resets after
 * hitting the fatal error
 * @param type see enum error_case_e, use FATAL_RANDOM for random
 */
void
fatal_errors_trigger(enum error_case_e type);

#endif
