//
// Created by Cyril on 07/07/2021.
//

#include <pb.h>
#include <pb_decode.h>
#include <mcu_messaging.pb.h>
#include <errors.h>
#include <logging.h>
#include <FreeRTOS.h>
#include <queue.h>
#include "deserializer.h"
#include "app_config.h"

static QueueHandle_t m_queue_handle = 0;

ret_code_t
deserializer_pop_blocking(McuMessage *data)
{
    ASSERT_BOOL(m_queue_handle != 0);

    if (xQueueReceive(m_queue_handle, data, portMAX_DELAY) != pdTRUE)
    {
        LOG_WARNING("Fetching data in empty waiting list");
        return RET_ERROR_NOT_FOUND;
    }

    return RET_SUCCESS;
}

uint32_t
deserializer_unpack_push(uint8_t *buffer, size_t length)
{
    ASSERT_BOOL(m_queue_handle != 0);

    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    McuMessage data = {0};

    bool status = pb_decode(&stream, McuMessage_fields, &data);
    if (status)
    {
        if (xQueueSendToBack(m_queue_handle, &data, 0) != pdTRUE)
        {
            return RET_ERROR_NO_MEM;
        }
    }
    else
    {
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

ret_code_t
deserializer_init(void)
{
    if (m_queue_handle != 0)
    {
        return RET_ERROR_INVALID_STATE;
    }

    m_queue_handle = xQueueCreate(DESERIALIZER_QUEUE_SIZE, sizeof(McuMessage));

    return RET_SUCCESS;
}