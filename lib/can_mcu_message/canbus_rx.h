#ifndef ORB_MCU_MAIN_APP_CANBUS_RX_H
#define ORB_MCU_MAIN_APP_CANBUS_RX_H

ret_code_t
canbus_rx_init(void (*in_handler)(McuMessage *msg));

#endif // ORB_MCU_MAIN_APP_CANBUS_RX_H
