#include "app_config.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include "temperature/temperature.h"
#include "ui/operator_leds/operator_leds.h"
#include "utils.h"
#include <app_assert.h>
#include <device.h>
#include <drivers/can.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(battery, CONFIG_BATTERY_LOG_LEVEL);

static const struct device *can_dev;
K_THREAD_STACK_DEFINE(can_battery_rx_thread_stack, THREAD_STACK_SIZE_BATTERY);
static struct k_thread rx_thread_data = {0};

// minimum voltage needed to boot the Orb during startup (in millivolts)
#define BATTERY_MINIMUM_VOLTAGE_STARTUP_MV 13500

// the time between sends of battery data to the Jetson
#define BATTERY_INFO_SEND_PERIOD_MS 1000

#define WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS 2000
#define WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS 100

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

static bool got_battery_voltage_can_message = false;

static void
publish_battery_voltages(void)
{
    static BatteryVoltage voltages;
    CRITICAL_SECTION_ENTER(k);
    voltages = (BatteryVoltage){
        .battery_cell1_mv = state_414.voltage_group_1,
        .battery_cell2_mv = state_414.voltage_group_2,
        .battery_cell3_mv = state_414.voltage_group_3,
        .battery_cell4_mv = state_414.voltage_group_4,
    };
    CRITICAL_SECTION_EXIT(k);
    LOG_DBG("Battery voltage: (%d, %d, %d, %d) mV", voltages.battery_cell1_mv,
            voltages.battery_cell2_mv, voltages.battery_cell3_mv,
            voltages.battery_cell4_mv);
    publish_new(&voltages, sizeof voltages, McuToJetson_battery_voltage_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_capacity(void)
{
    static BatteryCapacity battery_cap;

    // LOG_INF when battery capacity changes
    if (battery_cap.percentage != state_499.state_of_charge) {
        LOG_INF("Main battery: %u%%", state_499.state_of_charge);
    }

    CRITICAL_SECTION_ENTER(k);
    battery_cap.percentage = state_499.state_of_charge;
    CRITICAL_SECTION_EXIT(k);

    LOG_DBG("State of charge: %u%%", battery_cap.percentage);
    publish_new(&battery_cap, sizeof(battery_cap),
                McuToJetson_battery_capacity_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_is_charging(void)
{
    static BatteryIsCharging is_charging;

    // LOG_INF when battery is_charging changes
    if (is_charging.battery_is_charging !=
        (state_499.flags & BIT(IS_CHARGING_BIT))) {
        LOG_INF("Is charging: %s",
                state_499.flags & BIT(IS_CHARGING_BIT) ? "yes" : "no");
    }

    CRITICAL_SECTION_ENTER(k);
    is_charging.battery_is_charging = state_499.flags & BIT(IS_CHARGING_BIT);
    CRITICAL_SECTION_EXIT(k);

    LOG_DBG("Is charging? %s", is_charging.battery_is_charging ? "yes" : "no");
    publish_new(&is_charging, sizeof(is_charging),
                McuToJetson_battery_is_charging_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_cell_temperature(void)
{
    int16_t cell_temperature;

    CRITICAL_SECTION_ENTER(k);
    cell_temperature = state_415.cell_temperature;
    CRITICAL_SECTION_EXIT(k);

    LOG_DBG("Battery cell temperature: %u.%u°C", cell_temperature / 10,
            cell_temperature % 10);
    temperature_report(Temperature_TemperatureSource_BATTERY_CELL,
                       cell_temperature / 10);
}

static void
publish_battery_diagnostic_flags(void)
{
    static BatteryDiagnostic diag;
    CRITICAL_SECTION_ENTER(k);
    diag.flags = state_499.flags;
    CRITICAL_SECTION_EXIT(k);
    LOG_DBG("Battery diag flags: 0x%02x", diag.flags);
    publish_new(&diag, sizeof(diag), McuToJetson_battery_diag_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_pcb_temperature(void)
{
    int16_t pcb_temperature;
    CRITICAL_SECTION_ENTER(k);
    pcb_temperature = state_499.pcb_temperature;
    CRITICAL_SECTION_EXIT(k);
    LOG_DBG("Battery PCB temperature: %u.%u°C", pcb_temperature / 10,
            pcb_temperature % 10);
    temperature_report(Temperature_TemperatureSource_BATTERY_PCB,
                       pcb_temperature / 10);
}

static void
handle_499(struct zcan_frame *frame)
{
    CRITICAL_SECTION_ENTER(k);
    state_499 = *(struct battery_499_s *)frame->data;
    CRITICAL_SECTION_EXIT(k);
}

static void
handle_414(struct zcan_frame *frame)
{
    CRITICAL_SECTION_ENTER(k);
    got_battery_voltage_can_message = true;
    state_414 = *(struct battery_414_s *)frame->data;
    CRITICAL_SECTION_EXIT(k);
}

static void
handle_415(struct zcan_frame *frame)
{
    CRITICAL_SECTION_ENTER(k);
    state_415 = *(struct battery_415_s *)frame->data;
    CRITICAL_SECTION_EXIT(k);
}

const static struct battery_can_msg messages[] = {
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

static ret_code_t
setup_filters(void)
{
    int ret;

    for (size_t i = 0; i < ARRAY_SIZE(messages); ++i) {
        battery_can_filter.id = messages[i].can_id;
        ret = can_add_rx_filter(can_dev, message_checker, (void *)&messages[i],
                                &battery_can_filter);
        if (ret < 0) {
            LOG_ERR("Error adding can rx filter (%d)", ret);
            return RET_ERROR_INTERNAL;
        }
    }
    return RET_SUCCESS;
}

static void
battery_rx_thread()
{
    while (1) {
        publish_battery_voltages();
        publish_battery_capacity();
        publish_battery_is_charging();
        publish_battery_cell_temperature();
        publish_battery_diagnostic_flags();
        publish_battery_pcb_temperature();

        k_msleep(BATTERY_INFO_SEND_PERIOD_MS);
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

    ret = setup_filters();
    if (ret != RET_SUCCESS) {
        return ret;
    }

    uint32_t full_voltage = 0;

    for (size_t i = 0; i < WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS /
                               WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS;
         ++i) {
        full_voltage = state_414.voltage_group_1 + state_414.voltage_group_2 +
                       state_414.voltage_group_3 + state_414.voltage_group_4;
        if (full_voltage >= BATTERY_MINIMUM_VOLTAGE_STARTUP_MV) {
            break;
        }
        k_msleep(WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS);
    }

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

        bool got_battery_voltage_can_message_local;
        CRITICAL_SECTION_ENTER(k);
        got_battery_voltage_can_message_local = got_battery_voltage_can_message;
        CRITICAL_SECTION_EXIT(k);

        if (got_battery_voltage_can_message_local) {
            LOG_ERR_IMM("Low battery voltage, rebooting!");
            NVIC_SystemReset();
        } else {
            CRITICAL_SECTION_ENTER(k);
            // insert some fake values to keep orb-core happy
            state_414.voltage_group_1 = 4000;
            state_414.voltage_group_2 = 4000;
            state_414.voltage_group_3 = 4000;
            state_414.voltage_group_4 = 4000;
            state_499.state_of_charge = 100;
            CRITICAL_SECTION_EXIT(k);
            LOG_INF("We have power but no battery info. Booting anyway.");
        }
    } else {
        LOG_INF("Battery voltage is ok");
    }

    k_tid_t tid = k_thread_create(
        &rx_thread_data, can_battery_rx_thread_stack,
        K_THREAD_STACK_SIZEOF(can_battery_rx_thread_stack), battery_rx_thread,
        NULL, NULL, NULL, THREAD_PRIORITY_BATTERY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "battery");

    return RET_SUCCESS;
}
