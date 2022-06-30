#ifndef FIRMWARE_PUB_H
#define FIRMWARE_PUB_H

#include "mcu_messaging.pb.h"
#include <stddef.h>
#include <stdint.h>

/// Send new message by passing only the dynamic size payload and associated tag
/// \param payload McuToJetson's payload
/// \param size Size of payload
/// \param which_payload tag
/// \param remote_addr Address to send to
/// \return RET_ERROR_INVALID_PARAM or RET_SUCCESS
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr);

#endif // FIRMWARE_PUB_H
