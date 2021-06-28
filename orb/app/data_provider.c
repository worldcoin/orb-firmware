//
// Created by Cyril on 28/06/2021.
//

#include <mcu_messaging.pb.h>
#include <com.h>
#include "data_provider.h"

static DataHeader m_data = DataHeader_init_zero;

uint32_t
data_set_payload(uint16_t tag, void *data)
{
    bool new_data_event = true;

    switch (tag)
    {
        case McuToJetson_power_button_tag:
        {
            PowerButton button = *(PowerButton *) data;

            if (m_data.message.m_message.payload.power_button.pressed != button.pressed)
            {
                m_data.message.m_message.payload.power_button.pressed = button.pressed;
                new_data_event = true;
            }
        }
            break;

        case McuToJetson_battery_voltage_tag:
        {
            BatteryVoltage * battery = (BatteryVoltage *) data;
            if (m_data.message.m_message.payload.battery_voltage.battery_mvolts != battery->battery_mvolts)
            {
                m_data.message.m_message.payload.battery_voltage.battery_mvolts = battery->battery_mvolts;
                new_data_event = true;
            }
        }
            break;

        default:break;

    }

    if (new_data_event)
    {
        com_data_changed(tag);
    }

    return 0;
}

DataHeader *
data_get(void)
{
    return &m_data;
}