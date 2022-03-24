#ifndef FIRMWARE_ORB_APP_INCLUDE_ERRORS_H
#define FIRMWARE_ORB_APP_INCLUDE_ERRORS_H

#include "stdint.h"

typedef enum {
    RET_SUCCESS = 0x00,
    RET_ERROR_INTERNAL = 0x01,
    RET_ERROR_NO_MEM = 0x02,
    RET_ERROR_NOT_FOUND = 0x03,
    RET_ERROR_INVALID_PARAM = 0x04,
    RET_ERROR_INVALID_STATE = 0x05,
    RET_ERROR_INVALID_ADDR = 0x06,
    RET_ERROR_BUSY = 0x07,
    RET_ERROR_OFFLINE = 0x08,
    RET_ERROR_FORBIDDEN = 0x09,
    RET_ERROR_TIMEOUT = 0x0A,
    RET_ERROR_NOT_INITIALIZED = 0x0B,
    RET_ERROR_ASSERT_FAILS = 0x0C,
} ret_code_t;

void
app_assert_handler(int32_t error_code, uint32_t line_num,
                   const uint8_t *p_file_name);

uint32_t
app_assert_count();

#ifdef DEBUG
/**@brief Macro for calling error handler function if supplied error code any
 * other than 0.
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define APP_ASSERT(ERR_CODE)                                                   \
    do {                                                                       \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);                            \
        if (LOCAL_ERR_CODE != RET_SUCCESS) {                                   \
            app_assert_handler((ERR_CODE), __LINE__, (uint8_t *)__FILE__);     \
        }                                                                      \
    } while (0)

/**@brief Macro for calling error handler function if supplied boolean value is
 * false.
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#define APP_ASSERT_BOOL(BOOLEAN_VALUE)                                         \
    do {                                                                       \
        const uint32_t LOCAL_BOOLEAN_VALUE = (BOOLEAN_VALUE);                  \
        if (!LOCAL_BOOLEAN_VALUE) {                                            \
            app_assert_handler(0, __LINE__, (uint8_t *)__FILE__);              \
        }                                                                      \
    } while (0)

#else
#define APP_ASSERT(ERR_CODE)
#endif

#endif // FIRMWARE_ORB_APP_INCLUDE_ERRORS_H
