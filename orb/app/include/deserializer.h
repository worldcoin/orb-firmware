//
// Created by Cyril on 07/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
#define WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H

#include <stdint.h>
#include <stddef.h>

uint32_t
deserializer_unpack(uint8_t *buffer, size_t length);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_DESERIALIZER_H
