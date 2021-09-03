//
// Created by Cyril on 03/09/2021.
//

#include <FreeRTOS.h>
#include <task.h>
#include <serializer.h>
#include <can_bus.h>
#include <board.h>
#include <logging.h>
#include <app_config.h>
#include "sync.h"

static TaskHandle_t m_sync_task_handle = NULL;
static uint8_t m_data_protobuf_buffer[PROTOBUF_DATA_MAX_SIZE] =
    {0};  // for transmitted protobuf data


static void
tx_complete_cb(ret_code_t err_code)
{
    // check error code
    if (err_code == RET_SUCCESS)
    {
        LOG_INFO("TX complete");
    }
    else
    {
        // todo try to resend?
        LOG_ERROR("CAN TX error: %u", err_code);
    }

    // check if running from interrupt
    if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
    {
        BaseType_t switch_to_higher_priority_task = pdFALSE;
        vTaskNotifyGiveFromISR(m_sync_task_handle, &switch_to_higher_priority_task);
        portYIELD_FROM_ISR(switch_to_higher_priority_task);
    }
    else
    {
        xTaskNotifyGive(m_sync_task_handle);
    }
}

_Noreturn static void
sync_task(void *t)
{
    uint32_t ready = 0;
    ret_code_t err_code;

    // init as ready
    xTaskNotifyGive(m_sync_task_handle);

    while (1)
    {
        // block task while waiting for completion of last sent message
        ready = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));

        if (ready)
        {
            memset(m_data_protobuf_buffer, 0, sizeof m_data_protobuf_buffer);

            // wait for new data and pack it instantly
            // task will be put in blocking state until data is ready to be packed
            uint16_t length =
                (uint16_t) serializer_pull_next(&m_data_protobuf_buffer[0],
                                                sizeof m_data_protobuf_buffer);

            if (length != 0)
            {
                can_bind(CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES, tx_complete_cb, NULL);

                LOG_INFO("Sending protobuf message, len %u", length);

                err_code =
                    can_send(CAN_ID_MAIN_MCU_DATA_PROTOBUF_FRAMES, m_data_protobuf_buffer, length);
                ASSERT(err_code);
            }
            else
            {
                LOG_ERROR("Error with encoded frame, length: %u", length);
            }
        }
    }
}

void
sync_init(void)
{
    BaseType_t freertos_err_code = xTaskCreate(sync_task,
                                               "sync",
                                               512,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_sync_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}