//
// Created by Cyril on 07/07/2021.
//

#include "FreeRTOS.h"
#include <task.h>
#include <deserializer.h>
#include <logging.h>
#include <can_bus.h>
#include "control.h"

TaskHandle_t m_control_task_handle = NULL;

static void
rx_complete_cb(uint8_t *data, size_t len)
{
    ret_code_t err_code = deserializer_unpack_push(data, len);
    ASSERT(err_code);
}

_Noreturn static void
control_task(void *t)
{
    McuMessage data = {0};

    can_bind(CAN_ID_JETSON_COMMANDS_PROTOBUF_FRAMES, NULL, rx_complete_cb);

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
                    LOG_INFO("Brightness: %lu",
                             data.message.j_message.payload.brightness_front_leds.white_leds);
                }
                    break;

                default:
                {
                    LOG_ERROR("Unhandled control data type: 0x%x",
                              data.message.j_message.which_payload);
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
                                               250,
                                               NULL,
                                               (tskIDLE_PRIORITY + 2),
                                               &m_control_task_handle);
    ASSERT_BOOL(freertos_err_code == pdTRUE);
}