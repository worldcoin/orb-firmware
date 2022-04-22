#include "app_config.h"
#include "can_messaging.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include <assert.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(battery);
#include "temperature/temperature.h"
#include <device.h>
#include <drivers/can.h>
#include <sys/byteorder.h>
#include <zephyr.h>

static const struct device *can_dev;

// accept everything and discard unwanted messages in software
static const struct zcan_filter battery_can_filter = {
    .id_type = CAN_STANDARD_IDENTIFIER,
    .rtr = CAN_DATAFRAME,
    .id = 0,
    .rtr_mask = 1,
    .id_mask = 0};

#define BATTERY_FLAGS_BYTE_NUM             4
#define STATE_OF_CHARGE_BYTE_NUM           5
#define BATTERY_PCB_TEMPERATURE_WORD_INDEX 0
#define IS_CHARGING_BIT                    3

static void
handle_499(struct zcan_frame *frame)
{
    static McuMessage msg = {
        .which_message = McuMessage_m_message_tag,
    };

    if (can_dlc_to_bytes(frame->dlc) == 6) {
        // Let's extract the info we care about
        uint8_t state_of_charge = frame->data[STATE_OF_CHARGE_BYTE_NUM];
        bool is_charging =
            (frame->data[BATTERY_FLAGS_BYTE_NUM] & BIT(IS_CHARGING_BIT)) >>
            IS_CHARGING_BIT;
        uint16_t pcb_temp_tenths_of_c = sys_le16_to_cpu(
            ((uint16_t *)frame->data)[BATTERY_PCB_TEMPERATURE_WORD_INDEX]);

        // logging
        LOG_DBG("state of charge: %u%%", state_of_charge);
        LOG_DBG("is charging? %s", is_charging ? "yes" : "no");

        // now let's report the data
        msg.message.m_message.which_payload = McuToJetson_battery_capacity_tag;
        msg.message.m_message.payload.battery_capacity.percentage =
            state_of_charge;
        // don't care if message is dropped
        can_messaging_async_tx(&msg);

        msg.message.m_message.which_payload =
            McuToJetson_battery_is_charging_tag;
        msg.message.m_message.payload.battery_is_charging.battery_is_charging =
            is_charging;
        // don't care if message is dropped
        can_messaging_async_tx(&msg);

        LOG_DBG("Battery PCB temperature: %u.%u°C", pcb_temp_tenths_of_c / 10,
                pcb_temp_tenths_of_c % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_PCB,
                           pcb_temp_tenths_of_c / 10);
    } else {
        LOG_ERR(
            "Got state of charge and 'is charging' CAN frame (0x499), but it "
            "has the wrong length");
    }
}

#define BATTERY_CELL_TEMPERATURE_WORD_INDEX 1

static void
handle_415(struct zcan_frame *frame)
{
    if (can_dlc_to_bytes(frame->dlc) == 4) {
        uint16_t cell_temperature_tenths_of_c = sys_le16_to_cpu(
            ((uint16_t *)frame->data)[BATTERY_CELL_TEMPERATURE_WORD_INDEX]);
        LOG_DBG("Battery cell temperature: %u.%u°C",
                cell_temperature_tenths_of_c / 10,
                cell_temperature_tenths_of_c % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_CELL,
                           cell_temperature_tenths_of_c / 10);
    } else {
        LOG_ERR("Got 'battery current' and 'cell temperature' CAN frame "
                "(0x415), but it "
                "has the wrong length");
    }
}

static void
can_rx_callback(const struct device *dev, struct zcan_frame *frame,
                void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (frame->id) {
    case 0x499:
        handle_499(frame);
        break;
    case 0x415:
        handle_415(frame);
        break;
    }
}

ret_code_t
battery_init(void)
{
    int ret;

    can_dev = DEVICE_DT_GET(DT_ALIAS(battery_can_bus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN not ready");
        return RET_ERROR_BUSY;
    } else {
        LOG_INF("CAN ready");
    }

    ret =
        can_add_rx_filter(can_dev, can_rx_callback, NULL, &battery_can_filter);
    if (ret < 0) {
        LOG_ERR("Error attaching can message filter (%d)!", ret);
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}
