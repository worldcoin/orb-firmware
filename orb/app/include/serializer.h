//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
#define WORLDCOIN_FIRMWARE_ORB_APP_PACK_H

#include <stdint.h>
#include <stddef.h>
#include <mcu_messaging.pb.h>

/// Check if serializer is buffering some data
/// \return \c true is serializer is buffering data, \c false if empty
bool
serializer_data_waiting(void);

/// Push new structure into the serializer to be sent when possible
/// - Thread-safe
/// - Do not use from ISR
/// - Data is copied into internal buffer, ready to be packed, \see serializer_pack_next
/// \param data Pointer to structure to be serialized, will be copied into internal buffer
/// \return \c RET_SUCCESS on success, \c RET_ERROR_NO_MEM if serializer is full and data cannot be pushed.
uint32_t
serializer_push(DataHeader *data);

/// Pack queued data using Protobuf
/// \see serializer_push
/// \param buffer Pointer to buffer where the data is to be copied
/// \param len Maximum length of the \c buffer passed
/// \return size of used bytes into the buffer. 0 if there is no data to pack.
uint32_t
serializer_pack_next(uint8_t *buffer, size_t len);

void
serializer_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
