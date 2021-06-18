//
// Created by Cyril on 17/06/2021.
//

#ifndef FIRMWARE_ORB_APP_INCLUDE_ERRORS_H
#define FIRMWARE_ORB_APP_INCLUDE_ERRORS_H

#include "stdint.h"

/**@brief Structure containing info about an error of the type @ref NRF_FAULT_ID_SDK_ERROR.
 */
typedef struct
{
    uint32_t line_num;    //!< The line number where the error occurred.
    uint8_t const *p_file_name; //!< The file in which the error occurred.
    uint32_t err_code;    //!< The error code representing the error that occurred.
} error_info_t;

void
app_error_fault_handler(uint32_t id, uint32_t pc, long info);
void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name);

/**@brief Macro for calling error handler function.
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define APP_ERROR_HANDLER(ERR_CODE)                                    \
    do                                                                 \
    {                                                                  \
        app_error_handler((ERR_CODE), __LINE__, (const uint8_t*) __FILE__);  \
    } while (0)

/**@brief Macro for calling error handler function if supplied error code any other than VTC_SUCCESS.
 *
 * @param[in] ERR_CODE Error code supplied to the error handler.
 */
#define ASSERT(ERR_CODE)                                    \
    do                                                      \
    {                                                       \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);       \
        if (LOCAL_ERR_CODE != 0)                  \
        {                                                   \
            APP_ERROR_HANDLER(LOCAL_ERR_CODE);              \
        }                                                   \
    } while (0)

/**@brief Macro for calling error handler function if supplied boolean value is false.
 *
 * @param[in] BOOLEAN_VALUE Boolean value to be evaluated.
 */
#define ASSERT_BOOL(BOOLEAN_VALUE)                          \
    do                                                        \
    {                                                         \
        const uint32_t LOCAL_BOOLEAN_VALUE = (BOOLEAN_VALUE); \
        if (!LOCAL_BOOLEAN_VALUE)                             \
        {                                                     \
            APP_ERROR_HANDLER(1);        \
        }                                                     \
    } while (0)


#endif //FIRMWARE_ORB_APP_INCLUDE_ERRORS_H
