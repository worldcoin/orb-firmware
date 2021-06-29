//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
#define WORLDCOIN_FIRMWARE_ORB_APP_PACK_H

#include <stdint.h>
#include <stddef.h>
#include <mcu_messaging.pb.h>

uint32_t
serializer_push(DataHeader* data);

uint32_t
serializer_pack_waiting(uint8_t * buffer, size_t len);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
