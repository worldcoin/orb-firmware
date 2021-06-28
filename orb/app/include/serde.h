//
// Created by Cyril on 28/06/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
#define WORLDCOIN_FIRMWARE_ORB_APP_PACK_H

#include <stdint.h>
#include <stddef.h>

#define MAXIMUM_PACKED_SIZED_BYTES  128

uint32_t
serde_pack_payload_tag(uint16_t tag, uint8_t *buffer, size_t length);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_PACK_H
