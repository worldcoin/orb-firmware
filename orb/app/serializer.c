//
// Created by Cyril on 28/06/2021.
//

#include "serializer.h"
#include <pb.h>
#include <pb_encode.h>
#include <mcu_messaging.pb.h>
#include <logging.h>
#include <pb_decode.h>
#include <com.h>
#include <errors.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <app_config.h>

static QueueHandle_t m_queue_handle = 0;

static void
init_stream(McuMessage *pb_struct)
{
    pb_struct->version = Version_VERSION_0;
    pb_struct->which_message = McuMessage_m_message_tag;
}

static uint32_t
encode(pb_ostream_t *stream, McuMessage *pb_struct)
{
    if (!pb_encode(stream, McuMessage_fields, pb_struct))
    {
        LOG_ERROR("Unable to encode data");
        return 0;
    }
    return stream->bytes_written;
}

uint32_t
serializer_pull_next(uint8_t *buffer, size_t length)
{
    ASSERT_BOOL(m_queue_handle != 0);

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, length);

    McuMessage data = {0};

    if (xQueueReceive(m_queue_handle, &data, portMAX_DELAY) != pdTRUE)
    {
        LOG_WARNING("Fetching data in empty waiting list");
        return 0;
    }

    // data ready
    init_stream(&data);
    uint32_t size = encode(&stream, &data);

    return size;
}

ret_code_t
serializer_push(McuMessage *data)
{
    ASSERT_BOOL(m_queue_handle != 0);

    if (xQueueSendToBack(m_queue_handle, data, 0) != pdTRUE)
    {
        return RET_ERROR_NO_MEM;
    }

    return RET_SUCCESS;
}

ret_code_t
serializer_init(void)
{
    if (m_queue_handle != 0)
    {
        return RET_ERROR_INVALID_STATE;
    }

    m_queue_handle = xQueueCreate(SERIALIZER_QUEUE_SIZE, sizeof(McuMessage));

    return RET_SUCCESS;
}