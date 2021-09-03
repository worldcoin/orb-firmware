//
// Created by Cyril on 02/09/2021.
//

#ifndef ORB_FIRMWARE_ORB_APP_CAN_BUS_H
#define ORB_FIRMWARE_ORB_APP_CAN_BUS_H

#include <stdint.h>
#include <stddef.h>
#include <errors.h>

#define CAN_ID_BASE     0x100

typedef enum {
    CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES = 0x00,
    CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES = 0x01,
    CAN_ID_COUNT,
} can_id_t;

/// Register callbacks for messages using ID \c e_id
/// \param e_id Enum of the ID, actual used ID is (\c CAN_BASE_ID + \c e_id)
/// \param tx_complete_cb Will be called when message has been sent with ID \c e_id. Use \c can_send to send a message.
/// \param rx_complete_cb Will be called when message has been received with ID \c e_id. Binding activates continuous listening.
void
can_bind(can_id_t e_id, void (*tx_complete_cb)(ret_code_t), void (*rx_complete_cb)(uint8_t * data, size_t len));

/// Send message with ID = (\c CAN_BASE_ID + \c e_id)
/// \param e_id
/// \param data Pointer to data to send
/// \param len Length of data to send
/// \return Error code:
///  - \c RET_ERROR_INVALID_STATE: call can_bind before
///  - \c RET_ERROR_BUSY: message already being sent
///  - \c RET_SUCCESS: message will be sent
ret_code_t
can_send(can_id_t e_id, uint8_t *data, uint16_t len);

void
can_init(void);

#endif //ORB_FIRMWARE_ORB_APP_CAN_BUS_H
