//
// Created by Cyril on 28/06/2021.
//

#include "serializer.h"
#include <pb.h>
#include <pb_encode.h>
#include <mcu_messaging.pb.h>
#include <logging.h>
#include <pb_decode.h>
#include <data_provider.h>
#include <com.h>
#include <errors.h>

#define DATA_WAITING_LIST_SIZE 8
static DataHeader m_waiting_list[DATA_WAITING_LIST_SIZE] = {0};
static size_t m_wr_idx = 0;
static size_t m_rd_idx = 0;

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

bool
serializer_data_waiting(void)
{
    return (m_rd_idx != m_wr_idx);
}

uint32_t
serializer_pack_waiting(uint8_t * buffer, size_t len)
{
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, len);

    // check if waiting list is empty
    if (m_rd_idx != m_wr_idx)
    {
        init_stream(&m_waiting_list[m_rd_idx]);
        uint32_t size = encode(&stream, &m_waiting_list[m_rd_idx]);

        // push read index
        m_rd_idx = (m_rd_idx + 1) % DATA_WAITING_LIST_SIZE;

        return size;
    }
    else
    {
        LOG_WARNING("Fetching data in empty waiting list");
    }

    return 0;
}

uint32_t
serializer_push(DataHeader* data)
{
    if ((m_wr_idx + 1) % DATA_WAITING_LIST_SIZE != m_rd_idx)
    {
        memcpy(&m_waiting_list[m_wr_idx], data, sizeof(DataHeader));
        m_wr_idx = (m_wr_idx + 1) % DATA_WAITING_LIST_SIZE;

        com_new_data();

        return RET_SUCCESS;
    }
    else
    {
        LOG_ERROR("Waiting list full");

        return RET_ERROR_NO_MEM;
    }
}
