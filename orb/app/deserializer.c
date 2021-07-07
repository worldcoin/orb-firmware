//
// Created by Cyril on 07/07/2021.
//

#include <pb.h>
#include <pb_decode.h>
#include <mcu_messaging.pb.h>
#include <errors.h>
#include <logging.h>
#include "deserializer.h"

uint32_t
deserializer_unpack(uint8_t *buffer, size_t length)
{
    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    DataHeader data = {0};

    bool status = pb_decode(&stream, DataHeader_fields, &data);
    if (status)
    {
        LOG_INFO("Data: %lu", data.message.j_message.payload.brightness_front_leds.white_leds);
    }

    return RET_SUCCESS;
}