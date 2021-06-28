//
// Created by Cyril on 28/06/2021.
//

#include <errors.h>
#include <serde.h>
#include <logging.h>
#include <mcu_messaging.pb.h>
#include "FreeRTOS.h"
#include "task.h"
#include "com.h"

TaskHandle_t m_com_tx_task_handle = NULL;

/// Data types to be sent to Jetson
/// Indexes are used as a notification flag
/// ⚠️ Sort from most important priority to least important as they will be processed in that order
const uint16_t PAYLOAD_TAGS_PRIO_ORDER[] = {
    McuToJetson_power_button_tag,
    McuToJetson_battery_voltage_tag,
};

static_assert(sizeof(PAYLOAD_TAGS_PRIO_ORDER)/sizeof(uint16_t) <= 32, "Must not exceed 32");

static uint8_t m_buffer[MAXIMUM_PACKED_SIZED_BYTES] = {0};

/// The goal of that task is to:
/// - get the awaiting data to be sent
/// - pack the data (protobuf)
/// - send it over the UART link
/// Note. Packing could be done in another task
/// @param t unused at that point
_Noreturn static void
com_tx_task(void *t)
{
    uint32_t notifications = 0;

    while (1)
    {
        // get pending notifications
        // block task while waiting
        notifications = 0;
        BaseType_t err_code = xTaskNotifyWait(0, 0, &notifications, pdMS_TO_TICKS(1000));

        if (err_code && notifications)
        {
            LOG_INFO("notif: %lu", notifications);

            for (int i = 0; i < sizeof(PAYLOAD_TAGS_PRIO_ORDER)/sizeof(uint16_t); ++i)
            {
                uint32_t len = 0;

                if (notifications & (1 << i))
                {
                    // pack data
                    len = serde_pack_payload_tag(PAYLOAD_TAGS_PRIO_ORDER[i], m_buffer, sizeof m_buffer);

                    if (len != 0)
                    {
                        // clear
                        ulTaskNotifyValueClear(m_com_tx_task_handle, (1 << i));

                        LOG_INFO("Sending: t %u, l %luB", PAYLOAD_TAGS_PRIO_ORDER[i], len);
                        // todo send packed data
                    }
                }
            }
        }
    }
}

void
com_data_changed(uint16_t tag)
{
    // look for the tag in the list
    // and notify index of tag
    for (uint32_t i = 0; i < sizeof(PAYLOAD_TAGS_PRIO_ORDER) / sizeof(uint16_t); ++i)
    {
        if (PAYLOAD_TAGS_PRIO_ORDER[i] == tag)
        {
            xTaskNotify(m_com_tx_task_handle, (1 << i), eSetBits);
            return;
        }
    }

    LOG_ERROR("Cannot find tag: %u", tag);
}

uint32_t
com_init(void)
{
    BaseType_t err_code = xTaskCreate(com_tx_task, "com_tx", 150, NULL, (tskIDLE_PRIORITY + 1), &m_com_tx_task_handle);
    if (err_code != pdPASS)
    {
        ASSERT(1);
    }

    return 0;
}