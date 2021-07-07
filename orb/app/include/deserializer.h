//
// Created by Cyril on 07/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
#define WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H

#include <stdint.h>
#include <stddef.h>
#include <mcu_messaging.pb.h>
#include "errors.h"

ret_code_t
deserializer_pop_blocking(DataHeader *data);

uint32_t
deserializer_unpack_push(uint8_t *buffer, size_t length);

ret_code_t
deserializer_init(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
