//
// Created by Cyril on 28/06/2021.
//

#include <mcu_messaging.pb.h>
#include <serializer.h>
#include <errors.h>
#include <logging.h>
#include "data_provider.h"

ret_code_t
data_queue_message_payload(uint16_t tag, void *data, size_t size)
{
    ret_code_t err_code;

    McuMessage data_to_serialize = McuMessage_init_zero;

    // verify that we support passed tag
    switch (tag)
    {
        case McuToJetson_ack_tag             :
        case McuToJetson_power_button_tag    :
        case McuToJetson_battery_voltage_tag :
        case McuToJetson_battery_capacity_tag:
        case McuToJetson_tof_data_tag        :
        case McuToJetson_imu_data_tag        :
        case McuToJetson_mag_data_tag        :
        case McuToJetson_gps_tag             :
        case McuToJetson_fw_version_tag      :
        case McuToJetson_status_tag          :
        case McuToJetson_active_shutter_tag  :
        {
            err_code = RET_SUCCESS;
        }
            break;

        default:
        {
            err_code = RET_ERROR_INVALID_PARAM;
        }
            break;
    }

    if (err_code == RET_SUCCESS)
    {
        // copy payload and set tag
        memcpy((void *) &data_to_serialize.message.m_message.payload, data, size);
        data_to_serialize.message.m_message.which_payload = tag;

        // push to serializer
        err_code = serializer_push(&data_to_serialize);
    }

    return err_code;
}
