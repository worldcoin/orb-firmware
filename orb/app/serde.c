//
// Created by Cyril on 28/06/2021.
//

#include "serde.h"
#include <pb.h>
#include <pb_encode.h>
#include <mcu_messaging.pb.h>
#include <logging.h>
#include <pb_decode.h>
#include <data_provider.h>

static void
init_stream(DataHeader *pb_struct)
{
    pb_struct->version = Version_VERSION_0;
    pb_struct->which_message = DataHeader_m_message_tag;
}

static uint32_t
encode(pb_ostream_t *stream, DataHeader *pb_struct)
{
    if (!pb_encode(stream, DataHeader_fields, pb_struct))
    {
        LOG_ERROR("Unable to encode data");
        return 0;
    }
    return stream->bytes_written;
}

uint32_t
serde_pack_payload_tag(uint16_t tag, uint8_t *buffer, size_t length)
{
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, length);

    DataHeader* pb_struct = data_get();
    init_stream(pb_struct);

    pb_struct->message.m_message.which_payload = tag;

    uint32_t size = encode(&stream, pb_struct);
    return size;
}
