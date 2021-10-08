//
// Created by Cyril on 30/09/2021.
//

#ifndef ORB_FIRMWARE_APP_MAIN_SRC_CANBUS_H
#define ORB_FIRMWARE_APP_MAIN_SRC_CANBUS_H

#include <stddef.h>
#include "errors.h"

/// Send chunk of data over CAN bus
/// ISO-TP protocol is used to ensure flow-control
/// \param data
/// \param len
/// \param tx_complete_cb provide a callback function for non-blocking mode, NULL for blocking-mode
/// \return
/// * RET_SUCESS: success
/// * RET_ERROR_INTERNAL: iso-tp error
ret_code_t
canbus_send(const char *data, size_t len, void (*tx_complete_cb)(int, void *));

ret_code_t
canbus_init(void);

#endif //ORB_FIRMWARE_APP_MAIN_SRC_CANBUS_H
