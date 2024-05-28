#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if JetsonToMcu_size >= JetsonToSec_size &&                                    \
    JetsonToMcu_size >= McuToJetson_size &&                                    \
    JetsonToMcu_size >= SecToJetson_size
#define MCU_MESSAGE_ENCODED_WRAPPER_SIZE (McuMessage_size - JetsonToMcu_size)
#else
// A payload is larger than JetsonToMcu which does not allow to find the number
// of bytes needed to wrap the payload into an McuMessage. Please use another
// field to calculate the number of bytes needed to wrap the payload.
#error Unable to calculate bytes needed to wrap a payload into an McuMessage.
#endif

/**
 * @brief Starts publishing messages addressed to `remote_addr`
 *
 * @details In case some buffered messages are ready, a thread is spawned
 *     to send them to the remote address.
 * @param remote_addr Active remote address
 * @retval RET_SUCCESS publisher thread started
 * @retval RET_ERROR_NO_MEM too many remote addresses
 */
int
subscribe_add(uint32_t remote_addr);

/**
 * @brief Can be used to check if CAN communication to the Jetson is active
 *
 * @param remote Remote address
 * @retval true if the Jetson is ready to receive CAN messages
 * @retval false if not
 */
bool
publish_is_started(uint32_t remote);

/**
 * @brief Store message to send later
 *
 * @details Stored messages are sent when receiving a new message
 *      from remote to increase the chance of successfully transmitting
 *      the message, see `subscribe_add()` for more details.
 *
 * @param payload McuToJetson's payload
 * @param size Size of payload
 * @param which_payload tag
 * @param remote_addr Address to send to

 * @retval RET_SUCCESS message stored and will be sent later
 * @retval RET_ERROR_INVALID_PARAM one argument isn't supported
 */
int
publish_store(void *payload, size_t size, uint32_t which_payload,
              uint32_t remote_addr);

/**
 * @brief Send a new message
 *
 * Send new message by passing the payload and associated tag. If
 * Jetson isn't alive, the message might be stored locally for later
 * transmission
 *
 * @param payload McuToJetson's payload
 * @param size Size of payload
 * @param which_payload tag
 * @param remote_addr Address to send to
 *
 * @retval RET_SUCCESS message queued for sending
 * @retval RET_ERROR_OFFLINE depending on payload's priority, message is either
 *     discarded or stored
 * @retval RET_ERROR_INVALID_PARAM one argument isn't supported
 * @retval RET_ERROR_BUSY resource not available, likely taken by another
 * @retval RET_ERROR_INTERNAL error encoding message into Protobuf
 */
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr);

/**
 * @brief Publish all the stored messages and events
 *
 * Can be logs, diag, memfault, etc.
 */
void
publish_flush(void);
