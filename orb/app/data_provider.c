//
// Created by Cyril on 28/06/2021.
//

#include <mcu_messaging.pb.h>
#include <serializer.h>
#include <errors.h>
#include <logging.h>
#include "data_provider.h"

/// Set data that should be communicated:
///   - \c DataHeader.message.m_message.payload
/// \param tag Data type to be set (payload), \see mcu_messaging.pb.h
/// \param data Pointer to structure using the type defined in \see mcu_messaging.pb.h
/// \return
/// * \c RET_SUCCESS on success,
/// * \c RET_ERROR_NO_MEM when the queue keeping new data is full
/// * \c RET_ERROR_INVALID_PARAM when \param tag is unknown
uint32_t
data_set_payload(uint16_t tag, void *data)
{
    uint32_t err_code = RET_SUCCESS;

    bool new_data_event = false;
    DataHeader data_to_serialize = DataHeader_init_zero;

    switch (tag)
    {
        case McuToJetson_power_button_tag:
        {
            PowerButton button = *(PowerButton *) data;

            if (data_to_serialize.message.m_message.payload.power_button.pressed != button.pressed)
            {
                data_to_serialize.message.m_message.payload.power_button.pressed = button.pressed;
                new_data_event = true;
            }
        }
            break;

        case McuToJetson_battery_voltage_tag:
        {
            BatteryVoltage * battery = (BatteryVoltage *) data;
            if (data_to_serialize.message.m_message.payload.battery_voltage.battery_mvolts != battery->battery_mvolts)
            {
                data_to_serialize.message.m_message.payload.battery_voltage.battery_mvolts = battery->battery_mvolts;
                new_data_event = true;
            }
        }
            break;

        default:
        {
            LOG_ERROR("Unhandled: %u", tag);
            err_code = RET_ERROR_INVALID_PARAM;
        }
            break;
    }

    if (new_data_event)
    {
        data_to_serialize.message.m_message.which_payload = tag;
        err_code = serializer_push(&data_to_serialize);
    }

    return err_code;
}
