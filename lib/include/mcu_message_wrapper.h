#pragma once

/** The McuMessage has one, and only one, of a few different inner payloads,
 * and we want to know how many bytes are used to wrap that inner payload into
 * the McuMessage. Because all the payloads have a known size at compile time,
 * we can use the MAX macro to find the size of the largest payload and then
 * subtract that from the size of the McuMessage.
 */
#define MCU_MESSAGE_BIGGEST_PAYLOAD_SIZE                                       \
    MAX(MAX(MAX(orb_mcu_main_JetsonToMcu_size, orb_mcu_sec_JetsonToSec_size),  \
            orb_mcu_main_McuToJetson_size),                                    \
        orb_mcu_sec_SecToJetson_size)
#define MCU_MESSAGE_ENCODED_WRAPPER_SIZE                                       \
    (orb_mcu_McuMessage_size - MCU_MESSAGE_BIGGEST_PAYLOAD_SIZE)
