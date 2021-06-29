//
// Created by Cyril on 28/06/2021.
//

#include <errors.h>
#include <serializer.h>
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

static_assert(sizeof(PAYLOAD_TAGS_PRIO_ORDER) / sizeof(uint16_t) <= 32, "Must not exceed 32");

static uint8_t m_buffer[256] = {0};

/// The goal of that task is to:
/// - get the awaiting data to be sent
/// - pack the data (protobuf)
/// - send it over the UART link
/// @param t unused at that point
_Noreturn static void
com_tx_task(void *t)
{
    uint32_t notifications = 0;
    uint32_t length = 0;

    while (1)
    {
        // get pending notifications
        // block task while waiting
        notifications = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));
        length = 1;

        while (notifications-- && length)
        {
            // pack new waiting data
            length = serializer_pack_waiting(m_buffer,
                                             sizeof m_buffer);

            // append CRC16

            if (length != 0)
            {
                LOG_INFO("Sending: l %luB", length);
                // todo send packed data
            }
        }

        xTaskNotifyStateClear(m_com_tx_task_handle);
    }
}

void
com_new_data()
{
    xTaskNotifyGive(m_com_tx_task_handle);
}

uint32_t
com_init(void)
{
    BaseType_t err_code = xTaskCreate(com_tx_task,
                                      "com_tx",
                                      150,
                                      NULL,
                                      (tskIDLE_PRIORITY + 1),
                                      &m_com_tx_task_handle);
    if (err_code != pdPASS)
    {
        ASSERT(1);
    }

    return 0;
}