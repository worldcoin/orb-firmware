//
// Created by Cyril on 07/07/2021.
//

#include "FreeRTOS.h"
#include <task.h>
#include <deserializer.h>
#include <logging.h>
#include "control.h"

TaskHandle_t m_control_task_handle = NULL;

_Noreturn static void
control_task(void *t)
{
    DataHeader data = {0};

    while (1)
    {
        // wait for new control data
        // task will be put in blocking state until data is ready
        ret_code_t err_code =
            (uint16_t) deserializer_pop_blocking(&data);

        if (err_code == RET_SUCCESS)
        {
            switch (data.message.j_message.which_payload)
            {
                case JetsonToMcu_shutdown_tag:
                {
                    LOG_INFO("Shutdown");
                }
                    break;

                case JetsonToMcu_ir_leds_tag:
                {

                }
                break;

                case JetsonToMcu_brightness_front_leds_tag:
                {
                    LOG_INFO("Brightness: %lu", data.message.j_message.payload.brightness_front_leds.white_leds);
                }
                break;


                default:
                {
                    LOG_ERROR("Unhandled control data type: 0x%x", data.message.j_message.which_payload);
                }
            }
        }
    }
}

void
control_init(void)
{
    BaseType_t freertos_err_code = xTaskCreate(control_task,
                                               "control",
                                               150,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_control_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}