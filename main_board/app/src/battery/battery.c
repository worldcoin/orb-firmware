#include "app_config.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include "temperature/temperature.h"
#include "ui/operator_leds/operator_leds.h"
#include <app_assert.h>
#include <device.h>
#include <drivers/can.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(battery, CONFIG_BATTERY_LOG_LEVEL);

static const struct device *can_dev;

// minimum voltage needed to boot the Orb during startup (in millivolts)
#define BATTERY_MINIMUM_VOLTAGE_STARTUP_MV 13500

// the time between sends of battery data to the Jetson
#define BATTERY_INFO_SEND_PERIOD_MS 1000

K_SEM_DEFINE(first_414_wait_sem, 0, 1);

struct battery_can_msg {
    uint32_t can_id;
    uint8_t msg_len;
    void (*handler)(struct zcan_frame *frame);
};

static struct zcan_filter battery_can_filter = {.id_type =
                                                    CAN_STANDARD_IDENTIFIER,
                                                .rtr = CAN_DATAFRAME,
                                                .id = 0,
                                                .rtr_mask = 1,
                                                .id_mask = CAN_STD_ID_MASK};

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

__PACKED_STRUCT __may_alias battery_499_s
{
    int16_t pcb_temperature;  // unit 0.1ºC
    int16_t pack_temperature; // unit 0.1ºC
    uint8_t flags;
    uint8_t state_of_charge; // percentage
};

__PACKED_STRUCT __may_alias battery_414_s
{
    int16_t voltage_group_1; // unit milli-volts
    int16_t voltage_group_2; // unit milli-volts
    int16_t voltage_group_3; // unit milli-volts
    int16_t voltage_group_4; // unit milli-volts
};

static struct battery_499_s state_499 = {0};
static struct battery_414_s state_414 = {0};
static struct battery_415_s state_415 = {0};

static void
publish_battery_voltages(void)
{
    static BatteryVoltage voltages;
    voltages = (BatteryVoltage){
        .battery_cell1_mv = state_414.voltage_group_1,
        .battery_cell2_mv = state_414.voltage_group_2,
        .battery_cell3_mv = state_414.voltage_group_3,
        .battery_cell4_mv = state_414.voltage_group_4,
    };
    LOG_DBG("Battery voltage: (%d, %d, %d, %d) mV", state_414.voltage_group_1,
            state_414.voltage_group_2, state_414.voltage_group_3,
            state_414.voltage_group_4);
    publish_new(&voltages, sizeof voltages, McuToJetson_battery_voltage_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_capacity(void)
{
    static BatteryCapacity battery_cap;
    battery_cap.percentage = state_499.state_of_charge;
    LOG_DBG("State of charge: %u%%", state_499.state_of_charge);
    publish_new(&battery_cap, sizeof(battery_cap),
                McuToJetson_battery_capacity_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_is_charging(void)
{
    static BatteryIsCharging is_charging;
    is_charging.battery_is_charging = state_499.flags & BIT(IS_CHARGING_BIT);
    LOG_DBG("Is charging? %s", is_charging.battery_is_charging ? "yes" : "no");
    publish_new(&is_charging, sizeof(is_charging),
                McuToJetson_battery_is_charging_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_cell_temperature(void)
{
    LOG_DBG("Battery cell temperature: %u.%u°C",
            state_415.cell_temperature / 10, state_415.cell_temperature % 10);
    temperature_report(Temperature_TemperatureSource_BATTERY_CELL,
                       state_415.cell_temperature / 10);
}

static void
publish_battery_diagnostic_flags(void)
{
    static BatteryDiagnostic diag;
    diag.flags = state_499.flags;
    LOG_DBG("Battery diag flags: 0x%02x", diag.flags);
    publish_new(&diag, sizeof(diag), McuToJetson_battery_diag_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_pcb_temperature(void)
{
    LOG_DBG("Battery PCB temperature: %u.%u°C", state_499.pcb_temperature / 10,
            state_499.pcb_temperature % 10);
    temperature_report(Temperature_TemperatureSource_BATTERY_PCB,
                       state_499.pcb_temperature / 10);
}

static void
send_battery_info_to_jetson_work_func(struct k_work *work)
{
    ARG_UNUSED(work);

    publish_battery_voltages();
    publish_battery_capacity();
    publish_battery_is_charging();
    publish_battery_cell_temperature();
    publish_battery_diagnostic_flags();
    publish_battery_pcb_temperature();
}

K_WORK_DEFINE(battery_work, send_battery_info_to_jetson_work_func);

// This function is called from an interrupt context, so let's do the least
// amount of work possible in it
static void
send_battery_info_to_jetson_timer_func(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    k_work_submit(&battery_work);
}

K_TIMER_DEFINE(send_battery_info_timer, send_battery_info_to_jetson_timer_func,
               NULL);

static void
handle_499(struct zcan_frame *frame)
{
    state_499 = *(struct battery_499_s *)frame->data;
}

static void
handle_414(struct zcan_frame *frame)
{
    state_414 = *(struct battery_414_s *)frame->data;
}

static void
handle_415(struct zcan_frame *frame)
{
    state_415 = *(struct battery_415_s *)frame->data;
}

static struct battery_can_msg messages[] = {
    // 0x414 must be the first element of this array!
    {.can_id = 0x414, .msg_len = 8, .handler = handle_414},
    {.can_id = 0x415, .msg_len = 4, .handler = handle_415},
    {.can_id = 0x499, .msg_len = 6, .handler = handle_499}};

static void
message_checker(const struct device *dev, struct zcan_frame *frame,
                void *user_data)
{
    ARG_UNUSED(dev);
    struct battery_can_msg *msg = user_data;

    if (can_dlc_to_bytes(frame->dlc) == msg->msg_len) {
        msg->handler(frame);
    } else {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
    }
}

static void
sem_give_414(const struct device *dev, struct zcan_frame *frame,
             void *user_data)
{
    message_checker(dev, frame, user_data);
    k_sem_give(&first_414_wait_sem);
}

static ret_code_t
setup_filters(void)
{
    int ret;

    for (size_t i = 0; i < ARRAY_SIZE(messages); ++i) {
        battery_can_filter.id = messages[i].can_id;
        ret = can_add_rx_filter(can_dev, message_checker, &messages[i],
                                &battery_can_filter);
        if (ret < 0) {
            LOG_ERR("Error adding can rx filter (%d)", ret);
            return RET_ERROR_INTERNAL;
        }
    }
    return RET_SUCCESS;
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

    ASSERT_HARD_BOOL(messages[0].can_id == 0x414);

    battery_can_filter.id = 0x414;
    ret = can_add_rx_filter(can_dev, sem_give_414, &messages[0],
                            &battery_can_filter);
    if (ret < 0) {
        LOG_ERR("Error adding can rx filter (%d)", ret);
        return RET_ERROR_INTERNAL;
    }

    // we are simulating a blocking read with a timeout
    k_sem_take(&first_414_wait_sem, K_SECONDS(2));
    can_remove_rx_filter(can_dev, ret);

    // voltages received or timeout that will give a full_voltage of 0,
    // can be because of a removed battery
    uint32_t full_voltage =
        state_414.voltage_group_1 + state_414.voltage_group_2 +
        state_414.voltage_group_3 + state_414.voltage_group_4;
    LOG_INF("Got initial battery voltage: %umV", full_voltage);

    // if voltage low:
    // - show the user by blinking the operator LED in red
    // - reboot to allow for button startup again, hopefully with
    //   more charge
    if (full_voltage < BATTERY_MINIMUM_VOLTAGE_STARTUP_MV) {
        // blink in red before rebooting
        RgbColor color = {.red = 5, .green = 0, .blue = 0};
        for (int i = 0; i < 3; ++i) {
            operator_leds_blocking_set(&color, 0b11111);
            k_msleep(500);
            operator_leds_blocking_set(&color, 0b00000);
            k_msleep(500);
        }

        NVIC_SystemReset();
    } else {
        LOG_INF("Battery voltage is ok");
    }

    k_timer_start(&send_battery_info_timer, K_MSEC(BATTERY_INFO_SEND_PERIOD_MS),
                  K_MSEC(BATTERY_INFO_SEND_PERIOD_MS));

    return setup_filters();
}
