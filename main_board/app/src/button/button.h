#ifndef ORB_MCU_MAIN_APP_BUTTON_H
#define ORB_MCU_MAIN_APP_BUTTON_H

#include "errors.h"

int
button_uninit(void);

/**
 * @brief Initialize power button
 * Set up interrupt handling
 * @return
 * * RET_SUCCESS button is init
 * * RET_ERROR_INVALID_STATE device not ready
 * * RET_ERROR_INTERNAL error configuring interrupt on button event
 */
int
button_init(void);

#endif // ORB_MCU_MAIN_APP_BUTTON_H
