//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_COM_H
#define WORLDCOIN_FIRMWARE_ORB_APP_COM_H

#include <stdint.h>
#include <assert.h>

/// Tell com TX that new data is ready to be sent
/// This will unblock the TX task using a notification
/// \param tag gives the message's payload type to be sent, see \c mcu_messaging.pb.h
void
com_data_changed(uint16_t tag);

uint32_t
com_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_COM_H
