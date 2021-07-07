//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
#define WORLDCOIN_FIRMWARE_ORB_APP_PACK_H

#include <stdint.h>
#include <stddef.h>
#include <mcu_messaging.pb.h>
#include <errors.h>

/// Wait for data from the queue and pack it when available using Protobuf. The encoded stream is
/// put into the passed buffer.
/// ⚠️ This function will block the calling task
/// \see serializer_push to add data to the serializer
/// \param buffer Pointer to buffer where the data is to be copied
/// \param len Maximum length of the \c buffer passed
/// \return size of used bytes into the buffer. 0 if there is no data to pack.
uint32_t
serializer_pack_next_blocking(uint8_t *buffer, size_t len);

/// Push new structure into the serializer to be sent when possible
/// - Thread-safe
/// - Do not use from ISR
/// - Data is copied into internal buffer, ready to be packed, \see serializer_pack_next_blocking
/// \param data Pointer to structure to be serialized, will be copied into internal buffer
/// \return \c RET_SUCCESS on success, \c RET_ERROR_NO_MEM if serializer is full and data cannot be pushed.
ret_code_t
serializer_push(DataHeader *data);

/// Init serializer: create empty queue for message passing between producers and consumer
/// \return \c RET_SUCCESS on success, \c RET_ERROR_INVALID_STATE if already initialized,
ret_code_t
serializer_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
