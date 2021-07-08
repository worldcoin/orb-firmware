//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H
#define WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H

#include "mcu_messaging.pb.h"

/// Set data that should be communicated:
///   - \c McuMessage.message.m_message.payload
/// \param tag Data type to be set (payload), \see mcu_messaging.pb.h
/// \param data Pointer to structure using the type defined in \see mcu_messaging.pb.h
/// \return
/// * \c RET_SUCCESS on success,
/// * \c RET_ERROR_NO_MEM when the queue keeping new data is full
/// * \c RET_ERROR_INVALID_PARAM when \param tag is unknown
uint32_t
data_queue_message_payload(uint16_t tag, void * data);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_DATA_PROVIDER_H
