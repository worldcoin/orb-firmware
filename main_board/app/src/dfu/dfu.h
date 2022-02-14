//
// Created by Cyril on 12/11/2021.
//

#ifndef ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H
#define ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H

#include "utils.h"
#include <mcu_messaging.pb.h>
#include <stdbool.h>

#define DFU_BLOCK_SIZE_MAX                                                     \
    STRUCT_MEMBER_ARRAY_SIZE(FirmwareUpdateData, image_block.bytes)

int
dfu_load(uint32_t current_block_number, uint32_t block_count,
         const uint8_t *data, size_t size, uint32_t ack_number);

int
dfu_secondary_activate_permanently(void);

int
dfu_secondary_activate_temporarily(void);

int
dfu_secondary_check(uint32_t crc32);

int
dfu_primary_confirm(void);

int
dfu_versions_send(void);

int
dfu_init(void);

#endif // ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H
