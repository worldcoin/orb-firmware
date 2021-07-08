//
// Created by Cyril on 28/06/2021.
//

#include <mcu_messaging.pb.h>
#include <serializer.h>
#include <errors.h>
#include <logging.h>
#include "data_provider.h"


uint32_t
data_queue_message_payload(uint16_t tag, void *data)
{
    uint32_t err_code = RET_SUCCESS;

    McuMessage data_to_serialize = McuMessage_init_zero;

    switch (tag)
    {
        case McuToJetson_power_button_tag:
        {
            PowerButton button = *(PowerButton *) data;

            data_to_serialize.message.m_message.payload.power_button.pressed = button.pressed;
        }
            break;

        case McuToJetson_battery_voltage_tag:
        {
            BatteryVoltage * battery = (BatteryVoltage *) data;
            data_to_serialize.message.m_message.payload.battery_voltage.battery_mvolts = battery->battery_mvolts;
        }
            break;

        default:
        {
            LOG_ERROR("Unhandled: %u", tag);
            err_code = RET_ERROR_INVALID_PARAM;
        }
            break;
    }

    if (err_code == RET_SUCCESS)
    {
        data_to_serialize.message.m_message.which_payload = tag;
        err_code = serializer_push(&data_to_serialize);
    }

    return err_code;
}
