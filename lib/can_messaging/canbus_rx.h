#ifndef ORB_MCU_MAIN_APP_CANBUS_RX_H
#define ORB_MCU_MAIN_APP_CANBUS_RX_H

#include "can_messaging.h"

ret_code_t canbus_rx_init(ret_code_t (*in_handler)(void *message));

ret_code_t canbus_isotp_rx_init(ret_code_t (*in_handler)(void *message));

#endif // ORB_MCU_MAIN_APP_CANBUS_RX_H
