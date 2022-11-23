#ifndef FIRMWARE_PUBSUB_H
#define FIRMWARE_PUBSUB_H

#include <stddef.h>
#include <stdint.h>

void
publish_start(void);

/// \brief Store a message to be sent when remote is fully alive.
///
/// Stored messages are flushed when receiving a new message
/// from remote.
///
/// \param payload McuToJetson's payload
/// \param size Size of payload
/// \param which_payload tag
/// \param remote_addr Address to send to
/// \return
///     * RET_SUCCESS message stored and will be sent later
///     * RET_ERROR_INVALID_PARAM one argument isn't supported
int
publish_store(void *payload, size_t size, uint32_t which_payload,
              uint32_t remote_addr);

/// Send new message by passing only the dynamic size payload and associated tag
///
/// \param payload McuToJetson's payload
/// \param size Size of payload
/// \param which_payload tag
/// \param remote_addr Address to send to
/// \return
///     * RET_SUCCESS message queued for sending
///     * RET_ERROR_OFFLINE depending on payload's priority, message is either
///     discarded or stored
///     * RET_ERROR_INVALID_PARAM one argument isn't supported
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr);

#endif // FIRMWARE_PUBSUB_H
