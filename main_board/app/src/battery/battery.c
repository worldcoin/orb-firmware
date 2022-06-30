#include "app_config.h"
#include "can_messaging.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include "temperature/temperature.h"
#include <app_assert.h>
#include <device.h>
#include <drivers/can.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(battery);

static const struct device *can_dev;

// accept everything and discard unwanted messages in software
static const struct zcan_filter battery_can_filter = {
    .id_type = CAN_STANDARD_IDENTIFIER,
    .rtr = CAN_DATAFRAME,
    .id = 0,
    .rtr_mask = 1,
    .id_mask = 0};

__PACKED_STRUCT battery_415_s
{
    int16_t current_ma; // positive if current flowing into battery, negative if
                        // it flows out of it
    int16_t cell_temperature; // unit 0.1ºC
};

// clang-format off
// | Bit 7 | BQ769x2 reads valid | all recently read registers from bq769x2 where valid as of CRC and no timeout |
// | Bit 6 | USB PD ready | USB power delivery with ~20V established |
// | Bit 5 | USB PD initialized | USB power delivery periphery initialized |
// | Bit 4 | USB cable detected | USB cable plugged in and 5V present |
#define IS_CHARGING_BIT 3 // | Bit 3 | is Charging | USB PD is ready and charging current above 150 mA |
// | Bit 2 | Orb active | Host present and discharge current above 150 mA |
// | Bit 1 | Host Present | Battery is inserted, the host present pin is pulled low (high state) |
// | Bit 0 | User button pressed | User button on the battery is pressed |
// clang-format on
__PACKED_STRUCT battery_499_s
{
    int16_t pcb_temperature;  // unit 0.1ºC
    int16_t pack_temperature; // unit 0.1ºC
    uint8_t flags;
    uint8_t state_of_charge; // percentage
};

static struct battery_499_s state_499 = {0};

static void
handle_499(struct zcan_frame *frame)
{
    static BatteryCapacity battery_cap = {UINT32_MAX};
    static BatteryIsCharging is_charging = {.battery_is_charging = false};

    int ret = 0;

    if (can_dlc_to_bytes(frame->dlc) == 6) {
        // Let's extract the info we care about
        struct battery_499_s *new_state = (struct battery_499_s *)frame->data;

        // logging
        LOG_DBG("State of charge: %u%%", new_state->state_of_charge);
        LOG_DBG("Is charging? %s",
                is_charging.battery_is_charging ? "yes" : "no");

        // now let's report the data if it has changed
        if (state_499.state_of_charge != new_state->state_of_charge) {
            battery_cap.percentage = new_state->state_of_charge;

            ret |= publish_new(&battery_cap, sizeof(battery_cap),
                               McuToJetson_battery_capacity_tag,
                               CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }

        if ((new_state->flags & BIT(IS_CHARGING_BIT)) !=
            (state_499.flags & BIT(IS_CHARGING_BIT))) {
            is_charging.battery_is_charging =
                (new_state->flags & BIT(IS_CHARGING_BIT)) != 0;

            ret |= publish_new(&is_charging, sizeof(is_charging),
                               McuToJetson_battery_is_charging_tag,
                               CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        }

        // store new state only if messages successfully queued
        if (ret == RET_SUCCESS) {
            state_499 = *new_state;
        }

        LOG_DBG("Battery PCB temperature: %u.%u°C",
                new_state->pcb_temperature / 10,
                new_state->pcb_temperature % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_PCB,
                           new_state->pcb_temperature / 10);
    } else {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
    }
}

static void
handle_415(struct zcan_frame *frame)
{
    if (can_dlc_to_bytes(frame->dlc) == 4) {
        struct battery_415_s *new_state = (struct battery_415_s *)frame->data;
        LOG_DBG("Battery cell temperature: %u.%u°C",
                new_state->cell_temperature / 10,
                new_state->cell_temperature % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_CELL,
                           new_state->cell_temperature / 10);
    } else {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
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
