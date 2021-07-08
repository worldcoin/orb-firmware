//
// Created by Cyril on 07/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
#define WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H

#include <stdint.h>
#include <stddef.h>
#include <mcu_messaging.pb.h>
#include "errors.h"

/// Pop a \c DataHeader structure from the deserializer queue
/// Calling task will be put to blocked state while waiting
/// \param data Pointer to data structure to fill
/// \return \c RET_SUCCESS on success, \c RET_ERROR_NOT_FOUND on error
ret_code_t
deserializer_pop_blocking(DataHeader *data);

/// Parse new protobuf data and push it into deserializer internal queue,
/// \see deserializer_pop_blocking to get the parsed structure.
/// The internal queue has a size of \c DESERIALIZER_QUEUE_SIZE, \see app_config.h
/// \param buffer Pointer to protobuf byte array
/// \param length Length of the \c buffer
/// \return \c RET_SUCCESS on success, \c RET_ERROR_NO_MEM if there is no more memory available to push the new data
uint32_t
deserializer_unpack_push(uint8_t *buffer, size_t length);

ret_code_t
deserializer_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
