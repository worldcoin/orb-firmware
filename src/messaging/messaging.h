//
// Created by Cyril on 05/10/2021.
//

#ifndef ORB_FIRMWARE_APP_MAIN_SRC_MESSAGES_H
#define ORB_FIRMWARE_APP_MAIN_SRC_MESSAGES_H

#include <errors.h>
#include "mcu_messaging.pb.h"

void
messaging_push(McuMessage * message);

ret_code_t
messaging_init(void);

#endif //ORB_FIRMWARE_APP_MAIN_SRC_MESSAGES_H
