//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H
#define WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H

#include "mcu_messaging.pb.h"

/// Update newly updated data to be sent to Jetson
/// If data changes, the communication module will be warned that new data has to be sent
/// \param tag
/// \param data
/// \return
uint32_t
data_set_payload(uint16_t tag, void * data);

DataHeader *
data_get(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H
