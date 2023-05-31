#pragma once

#include "compilers.h"
#include "stdint.h"

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
 */
void
app_assert_soft_handler(int32_t error_code, uint32_t line_num,
                        const uint8_t *p_file_name);

/**@brief Macro for calling error handler function if supplied error code any
 * other than 0.
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define ASSERT_SOFT(ERR_CODE)                                                  \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            app_assert_soft_handler((ERR_CODE), __LINE__,                      \
                                    (uint8_t *)__FILE__);                      \
        }                                                                      \
    } while (0)

/**@brief Macro for calling error handler function if supplied error code any
 * other than 0.
 * Won't return if error, see app_assert_hard_handler
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define ASSERT_HARD(ERR_CODE)                                                  \
    do {                                                                       \
        if (ERR_CODE != 0) {                                                   \
            app_assert_hard_handler((ERR_CODE), __LINE__,                      \
                                    (uint8_t *)__FILE__);                      \
        }                                                                      \
    } while (0)

/**@brief Macro for calling error handler function if supplied boolean value is
 * false.
 * Won't return if error
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#define ASSERT_HARD_BOOL(BOOLEAN_VALUE)                                        \
    do {                                                                       \
        if (!(BOOLEAN_VALUE)) {                                                \
            app_assert_hard_handler(0, __LINE__, (uint8_t *)__FILE__);         \
        }                                                                      \
    } while (0)

/**@brief Macro for calling error handler function if supplied boolean value is
 * false.
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#define ASSERT_SOFT_BOOL(BOOLEAN_VALUE)                                        \
    do {                                                                       \
        if (!(BOOLEAN_VALUE)) {                                                \
            app_assert_soft_handler(0, __LINE__, (uint8_t *)__FILE__);         \
        }                                                                      \
    } while (0)

#else
#define ASSERT_SOFT(ERR_CODE)      UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_HARD(ERR_CODE)      UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_HARD_BOOL(ERR_CODE) UNUSED_VARIABLE(ERR_CODE)
#define ASSERT_SOFT_BOOL(ERR_CODE) UNUSED_VARIABLE(ERR_CODE)
#endif

#ifdef CONFIG_ORB_LIB_ERRORS_TESTS

/// Test a fatal error condition
/// Does not return: make sure the microcontroller resets after
/// hitting the fatal error
void
fatal_errors_test(void);

#endif
