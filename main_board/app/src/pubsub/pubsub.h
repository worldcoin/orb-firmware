#ifndef FIRMWARE_PUBSUB_H
#define FIRMWARE_PUBSUB_H

#include <stddef.h>
#include <stdint.h>

void
publish_start(void);

/// \brief Store message to send later
///
/// Stored messages are sent when receiving a new message
/// from remote to increase the chance of successfully transmitting
/// the message.
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

/// \brief Send a new message
///
/// Send new message by passing the payload and associated tag. If
/// Jetson isn't alive, the message might be stored locally for later
/// transmission
///
/// \param payload McuToJetson's payload
/// \param size Size of payload
/// \param which_payload tag
/// \param remote_addr Address to send to
/// \return RET_SUCCESS message queued for sending
/// \return RET_ERROR_OFFLINE depending on payload's priority, message is either
///     discarded or stored
/// \return RET_ERROR_INVALID_PARAM one argument isn't supported
/// \return RET_ERROR_BUSY resource not available, likely taken by another thread
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr);

#endif // FIRMWARE_PUBSUB_H
