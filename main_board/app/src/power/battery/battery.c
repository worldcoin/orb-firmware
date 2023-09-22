#include "app_config.h"
#include "battery_can.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "temperature/sensors/temperature.h"
#include "ui/operator_leds/operator_leds.h"
#include "utils.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include "logs_can.h"
LOG_MODULE_REGISTER(battery, CONFIG_BATTERY_LOG_LEVEL);

static const struct device *can_dev = DEVICE_DT_GET(DT_ALIAS(battery_can_bus));
K_THREAD_STACK_DEFINE(can_battery_rx_thread_stack, THREAD_STACK_SIZE_BATTERY);
static struct k_thread rx_thread_data = {0};

// minimum voltage needed to boot the Orb during startup (in millivolts)
#define BATTERY_MINIMUM_VOLTAGE_STARTUP_MV       13750
#define BATTERY_MINIMUM_VOLTAGE_RUNTIME_MV       12500
#define BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT 5

// the time between sends of battery data to the Jetson
// We selected 1100 ms because the battery publishes its data with 1000 ms
// Consequently we can be sure that at least one update was sent by the battery
// and the firmware doesn't assume falsely that the battery got removed.
#define BATTERY_INFO_SEND_PERIOD_MS 1100
#define BATTERY_MESSAGES_TIMEOUT_MS (BATTERY_INFO_SEND_PERIOD_MS * 8)
static_assert(
    BATTERY_MESSAGES_TIMEOUT_MS > BATTERY_INFO_SEND_PERIOD_MS * 3,
    "Coarse timing resolution to check if battery is still sending messages");

#define WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS 2000
#define WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS 100

volatile enum can_state current_can_controller_state = CAN_STATE_STOPPED;
volatile bool current_can_controller_state_changed = false;

static struct can_filter battery_can_filter = {
    .id = 0, .mask = CAN_STD_ID_MASK, .flags = CAN_FILTER_DATA};

static volatile bool transmission_completed = true;

#define CAN_MESSAGE_HANDLER(id)                                                \
    static struct battery_##id##_s state_##id = {0};                           \
    static volatile bool can_message_##id##_received = false;                  \
    static void handle_##id(struct can_frame *frame)                           \
    {                                                                          \
        CRITICAL_SECTION_ENTER(k);                                             \
        state_##id = *(struct battery_##id##_s *)frame->data;                  \
        can_message_##id##_received = true;                                    \
        CRITICAL_SECTION_EXIT(k);                                              \
    }

// NOLINTBEGIN - ignore clang-tidy warnings about empty statements
CAN_MESSAGE_HANDLER(400);
CAN_MESSAGE_HANDLER(410);
CAN_MESSAGE_HANDLER(411);
CAN_MESSAGE_HANDLER(412);
CAN_MESSAGE_HANDLER(414);
CAN_MESSAGE_HANDLER(415);
CAN_MESSAGE_HANDLER(490);
CAN_MESSAGE_HANDLER(491);
CAN_MESSAGE_HANDLER(492);
CAN_MESSAGE_HANDLER(499);
CAN_MESSAGE_HANDLER(522);
CAN_MESSAGE_HANDLER(523);
CAN_MESSAGE_HANDLER(524);
CAN_MESSAGE_HANDLER(525);
// NOLINTEND

struct battery_can_msg {
    uint32_t can_id;
    uint8_t msg_len;
    void (*handler)(struct can_frame *frame);
};

#define BATTERY_CAN_MESSAGE(id)                                                \
    {                                                                          \
        .can_id = 0x##id, .msg_len = sizeof(struct battery_##id##_s),          \
        .handler = handle_##id                                                 \
    }

static const struct battery_can_msg messages[] = {
    BATTERY_CAN_MESSAGE(400), BATTERY_CAN_MESSAGE(410),
    BATTERY_CAN_MESSAGE(411), BATTERY_CAN_MESSAGE(412),
    BATTERY_CAN_MESSAGE(414), BATTERY_CAN_MESSAGE(415),
    BATTERY_CAN_MESSAGE(490), BATTERY_CAN_MESSAGE(491),
    BATTERY_CAN_MESSAGE(492), BATTERY_CAN_MESSAGE(499),
    BATTERY_CAN_MESSAGE(522), BATTERY_CAN_MESSAGE(523),
    BATTERY_CAN_MESSAGE(524), BATTERY_CAN_MESSAGE(525),
};

static bool dev_mode = false;

static bool publish_battery_info_request = false;

/// EV1/EV2 batteries won't respond to RTR messages so cap the number of
/// attempts so that we don't put the CAN bus into a bad state
#define BATTERY_ID_REQUEST_ATTEMPTS 3
/// request battery metadata (ID, FW/HW versions...) if we detect that
/// the battery has been swapped or when the percentage changes
static int request_battery_info_left_attempts = BATTERY_ID_REQUEST_ATTEMPTS;

static void
battery_low_operator_leds_blink(void)
{
    RgbColor color = {.red = 5, .green = 0, .blue = 0};
    for (int i = 0; i < 3; ++i) {
        operator_leds_blocking_set(&color, 0b11111);
        k_msleep(500);
        operator_leds_blocking_set(&color, 0b00000);
        k_msleep(500);
    }
}

static void
publish_battery_reset_reason(void)
{
    static BatteryResetReason reset_reason;
    CRITICAL_SECTION_ENTER(k);
    reset_reason.reset_reason = state_400.reset_reason;
    CRITICAL_SECTION_EXIT(k);
    LOG_DBG("Battery reset reason: %d", reset_reason.reset_reason);
    int ret = publish_new(&reset_reason, sizeof reset_reason,
                          McuToJetson_battery_reset_reason_tag,
                          CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    if (ret != RET_SUCCESS) {
        LOG_DBG("Battery reset reason publish error: %d", ret);
    }
}

static void
publish_battery_voltages(void)
{
    if (can_message_414_received) {
        static BatteryVoltage voltages;
        CRITICAL_SECTION_ENTER(k);
        voltages = (BatteryVoltage){
            .battery_cell1_mv = state_414.voltage_group_1,
            .battery_cell2_mv = state_414.voltage_group_2,
            .battery_cell3_mv = state_414.voltage_group_3,
            .battery_cell4_mv = state_414.voltage_group_4,
        };
        CRITICAL_SECTION_EXIT(k);
        LOG_DBG("Battery voltage: (%d, %d, %d, %d) mV",
                voltages.battery_cell1_mv, voltages.battery_cell2_mv,
                voltages.battery_cell3_mv, voltages.battery_cell4_mv);
        publish_new(&voltages, sizeof voltages, McuToJetson_battery_voltage_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    }
}

static void
publish_battery_capacity(void)
{
    if (can_message_499_received) {
        static BatteryCapacity battery_cap;

        // LOG_INF when battery capacity changes
        if (battery_cap.percentage != state_499.state_of_charge) {
            LOG_INF("Main battery: %u%%", state_499.state_of_charge);
            request_battery_info_left_attempts = BATTERY_ID_REQUEST_ATTEMPTS;
        }

        CRITICAL_SECTION_ENTER(k);
        battery_cap.percentage = state_499.state_of_charge;
        CRITICAL_SECTION_EXIT(k);

        LOG_DBG("State of charge: %u%%", battery_cap.percentage);
        publish_new(&battery_cap, sizeof(battery_cap),
                    McuToJetson_battery_capacity_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    }
}

static void
publish_battery_is_charging(void)
{
    if (can_message_499_received) {
        static BatteryIsCharging is_charging;

        // LOG_INF when battery is_charging changes
        if (is_charging.battery_is_charging !=
            (state_499.flags & BIT(IS_CHARGING_BIT))) {
            LOG_INF("Is charging: %s",
                    state_499.flags & BIT(IS_CHARGING_BIT) ? "yes" : "no");
        }

        CRITICAL_SECTION_ENTER(k);
        is_charging.battery_is_charging =
            state_499.flags & BIT(IS_CHARGING_BIT);
        CRITICAL_SECTION_EXIT(k);

        LOG_DBG("Is charging? %s",
                is_charging.battery_is_charging ? "yes" : "no");
        publish_new(&is_charging, sizeof(is_charging),
                    McuToJetson_battery_is_charging_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    }
}

static void
publish_battery_cell_temperature(void)
{
    if (can_message_415_received) {
        int16_t cell_temperature;

        CRITICAL_SECTION_ENTER(k);
        cell_temperature = state_415.cell_temperature;
        CRITICAL_SECTION_EXIT(k);

        LOG_DBG("Battery cell temperature: %u.%u°C", cell_temperature / 10,
                cell_temperature % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_CELL,
                           cell_temperature / 10);
    }
}

static void
publish_battery_diagnostics(void)
{
    if (can_message_410_received && can_message_411_received &&
        can_message_412_received && can_message_415_received &&
        can_message_499_received) {
        static BatteryDiagnosticCommon diag_common = {0};
        static BatteryDiagnosticSafety diag_safety = {0};
        static BatteryDiagnosticPermanentFail diag_permanent_fail = {0};
        CRITICAL_SECTION_ENTER(k);
        diag_common.flags = state_499.flags;
        diag_common.bq769_control_status = state_410.bq769_control_status;
        diag_common.battery_status = state_410.battery_status;
        diag_common.fet_status = state_410.fet_status;
        diag_common.balancer_state = state_410.balancer_state;
        diag_common.current_ma = state_415.current_ma;

        diag_safety.safety_alert_a = state_411.safety_alert_a;
        diag_safety.safety_status_a = state_411.safety_status_a;
        diag_safety.safety_alert_b = state_411.safety_alert_b;
        diag_safety.safety_status_b = state_411.safety_status_b;
        diag_safety.safety_alert_c = state_411.safety_alert_c;
        diag_safety.safety_status_c = state_411.safety_status_c;

        diag_permanent_fail.permanent_fail_alert_a =
            state_412.permanent_fail_alert_a;
        diag_permanent_fail.permanent_fail_status_a =
            state_412.permanent_fail_status_a;
        diag_permanent_fail.permanent_fail_alert_b =
            state_412.permanent_fail_alert_b;
        diag_permanent_fail.permanent_fail_status_b =
            state_412.permanent_fail_status_b;
        diag_permanent_fail.permanent_fail_alert_c =
            state_412.permanent_fail_alert_c;
        diag_permanent_fail.permanent_fail_status_c =
            state_412.permanent_fail_status_c;
        diag_permanent_fail.permanent_fail_alert_d =
            state_412.permanent_fail_alert_d;
        diag_permanent_fail.permanent_fail_status_d =
            state_412.permanent_fail_status_d;
        CRITICAL_SECTION_EXIT(k);
        int ret = publish_new(&diag_common, sizeof(diag_common),
                              McuToJetson_battery_diag_common_tag,
                              CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        if (ret != RET_SUCCESS) {
            LOG_DBG("Battery diagnostics publish error diag_common: %d", ret);
        }

        ret = publish_new(&diag_safety, sizeof(diag_safety),
                          McuToJetson_battery_diag_safety_tag,
                          CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        if (ret != RET_SUCCESS) {
            LOG_DBG("Battery diagnostics publish error diag_safety: %d", ret);
        }

        ret = publish_new(&diag_permanent_fail, sizeof(diag_permanent_fail),
                          McuToJetson_battery_diag_permanent_fail_tag,
                          CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
        if (ret != RET_SUCCESS) {
            LOG_DBG("Battery diagnostics publish error diag_permanent_fail: %d",
                    ret);
        }

        LOG_DBG("Publishing battery diagnostics");
    }
}

static void
publish_battery_info(void)
{
    static BatteryInfoHwFw info_hw_fw;
    static BatteryInfoMaxValues info_max_values;
    static BatteryInfoSocAndStatistics info_soc_and_statistics;

    uint32_t commit_hash = (uint32_t)strtol(state_523.git_hash, NULL, 16);
    LOG_INF("Firmware Hash: 0x%08x", commit_hash);

    CRITICAL_SECTION_ENTER(k);
    info_hw_fw.mcu_id.bytes[0] = state_524.battery_mcu_id_bit_31_0;
    info_hw_fw.mcu_id.bytes[1] = state_524.battery_mcu_id_bit_31_0 >> 8;
    info_hw_fw.mcu_id.bytes[2] = state_524.battery_mcu_id_bit_31_0 >> 16;
    info_hw_fw.mcu_id.bytes[3] = state_524.battery_mcu_id_bit_31_0 >> 24;
    info_hw_fw.mcu_id.bytes[4] = state_525.battery_mcu_id_bit_63_32;
    info_hw_fw.mcu_id.bytes[5] = state_525.battery_mcu_id_bit_63_32 >> 8;
    info_hw_fw.mcu_id.bytes[6] = state_525.battery_mcu_id_bit_63_32 >> 16;
    info_hw_fw.mcu_id.bytes[7] = state_525.battery_mcu_id_bit_63_32 >> 24;
    info_hw_fw.mcu_id.bytes[8] = state_525.battery_mcu_id_bit_95_64;
    info_hw_fw.mcu_id.bytes[9] = state_525.battery_mcu_id_bit_95_64 >> 8;
    info_hw_fw.mcu_id.bytes[10] = state_525.battery_mcu_id_bit_95_64 >> 16;
    info_hw_fw.mcu_id.bytes[11] = state_525.battery_mcu_id_bit_95_64 >> 24;
    info_hw_fw.mcu_id.size = 12;

    info_hw_fw.hw_version =
        BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_UNDETECTED;
    if (can_message_522_received) {
        // Message 0x522 is only available on EV3 and later
        info_hw_fw.hw_version = state_522.hardware_version;
    } else if (can_message_491_received) {
        // On EV2 the hardware version is stored in message 0x491
        info_hw_fw.hw_version = state_491.detected_hardware_revision;
    }
    info_hw_fw.has_fw_version = true;
    info_hw_fw.fw_version.major = state_522.firmware_version_main;
    info_hw_fw.fw_version.minor = state_522.firmware_version_major;
    info_hw_fw.fw_version.patch = state_522.firmware_version_minor;
    info_hw_fw.fw_version.commit_hash = commit_hash;

    if (can_message_492_received) {
        info_soc_and_statistics.soc_state = state_492.soc_state;
        info_soc_and_statistics.soc_calibration =
            state_492.soc_calibration_state;
    } else {
        info_soc_and_statistics.soc_state =
            BatteryInfoSocAndStatistics_SocState_STATE_SOC_UNKNOWN;
        info_soc_and_statistics.soc_calibration =
            BatteryInfoSocAndStatistics_SocCalibration_STATE_SOC_CAL_UNKNOWN;
    }

    info_soc_and_statistics.number_of_charges = state_490.number_of_charges;
    info_soc_and_statistics.number_of_written_flash_variables =
        state_491.number_of_written_flash_variables_15_0 |
        ((uint32_t)(state_491.number_of_written_flash_variables_23_16) << 16);
    info_soc_and_statistics.number_of_button_presses =
        state_492.total_number_of_button_presses_15_0 |
        ((uint32_t)(state_492.total_number_of_button_presses_23_16) << 16);
    info_soc_and_statistics.number_of_insertions =
        state_492.number_of_insertions_15_0 |
        ((uint32_t)(state_492.number_of_insertions_23_16) << 16);

    info_max_values.maximum_capacity_mah = state_490.maximum_capacity_mah;
    info_max_values.maximum_cell_temp_decidegrees =
        state_490.maximum_cell_temp_deg_by_10;
    info_max_values.maximum_pcb_temp_decidegrees =
        state_490.maximum_pcb_temp_deg_by_10;
    info_max_values.maximum_charge_current_ma =
        state_491.maximum_charge_current_ma;
    info_max_values.maximum_discharge_current_ma =
        state_491.maximum_discharge_current_ma;
    CRITICAL_SECTION_EXIT(k);
    LOG_DBG("flash variables: %d",
            info_soc_and_statistics.number_of_written_flash_variables);

    int ret = publish_new(&info_hw_fw, sizeof(info_hw_fw),
                          McuToJetson_battery_info_hw_fw_tag,
                          CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    if (ret != RET_SUCCESS) {
        LOG_DBG("Battery info_one publish error: %d", ret);
    }

    ret = publish_new(&info_soc_and_statistics, sizeof(info_soc_and_statistics),
                      McuToJetson_battery_info_soc_and_statistics_tag,
                      CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    if (ret != RET_SUCCESS) {
        LOG_DBG("Battery info_two publish error: %d", ret);
    }

    ret = publish_new(&info_max_values, sizeof(info_max_values),
                      McuToJetson_battery_info_max_values_tag,
                      CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    if (ret != RET_SUCCESS) {
        LOG_DBG("Battery info_three publish error: %d", ret);
    }

    LOG_DBG("Publishing battery info");
}

static void
publish_battery_pcb_temperature(void)
{
    if (can_message_499_received) {
        int16_t pcb_temperature;
        CRITICAL_SECTION_ENTER(k);
        pcb_temperature = state_499.pcb_temperature;
        CRITICAL_SECTION_EXIT(k);
        LOG_DBG("Battery PCB temperature: %u.%u°C", pcb_temperature / 10,
                pcb_temperature % 10);
        temperature_report(Temperature_TemperatureSource_BATTERY_PCB,
                           pcb_temperature / 10);
    }
}

static void
publish_battery_pack_temperature(void)
{
    if (can_message_499_received) {
        int16_t pack_temperature;
        CRITICAL_SECTION_ENTER(k);
        pack_temperature = state_499.pack_temperature;
        CRITICAL_SECTION_EXIT(k);

        temperature_report(Temperature_TemperatureSource_BATTERY_PACK,
                           pack_temperature / 10);
    }
}

static void
battery_rtr_tx_complete_cb(const struct device *dev, int error_nr, void *arg)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(arg);

    if (error_nr != 0) {
        if (error_nr == -ENETDOWN) {
            // CAN controller is in stop state
            // this happens during normal operation if the Battery doesn't
            // respond to RTR messages
            LOG_DBG("RTR callback: ENETDOWN error");
        } else {
            ASSERT_SOFT(error_nr);
        }
    }

    transmission_completed = true;
}

static void
can_restart(void)
{
    LOG_INF("can_restart");

    int ret = can_stop(can_dev);
    if (ret != 0) {
        LOG_DBG("can_stop error: %d", ret);
        ASSERT_SOFT(ret);
    }
    k_msleep(500);
    ret = can_start(can_dev);
    if (ret != 0) {
        LOG_DBG("can_start error: %d", ret);
        ASSERT_SOFT(ret);
    }
    k_msleep(500);
}

static ret_code_t
send_rtr_message(uint32_t message_id)
{
    struct can_frame frame;
    frame.id = message_id;
    frame.dlc = 0;
    frame.flags = CAN_FRAME_RTR;

    LOG_DBG("can_send 0x%08x", message_id);
    transmission_completed = false;
    int ret = can_send(can_dev, &frame, K_MSEC(100), battery_rtr_tx_complete_cb,
                       NULL);
    if (ret != 0) {
        if (ret == -ENETUNREACH) {
            // CAN controller is in bus-off state
            LOG_DBG("!!! ENETUNREACH (bus-off)");
            // Using can_recover() from the Zephyr driver seems obvious but this
            // didn't work reliably during testing More reliable: restart the
            // whole driver
            can_restart();
            return RET_ERROR_INTERNAL;
        } else if (ret == -EAGAIN) {
            // timeout
            LOG_DBG("!!! EAGAIN (timeout)");
            can_restart();
            return RET_ERROR_INTERNAL;
        } else if (ret == -ENETDOWN) {
            // CAN controller is in stopped state
            LOG_DBG("!!! ENETDOWN (controller in stopped state)");
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        } else {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }
    }

    int16_t transmission_complete_timeout_ms = 100;

    while (!transmission_completed) {
        k_msleep(1);

        transmission_complete_timeout_ms--;

        if (current_can_controller_state_changed) {
            current_can_controller_state_changed = false;
            LOG_DBG("<!> can state changed to: %d",
                    current_can_controller_state);
            if (current_can_controller_state == CAN_STATE_BUS_OFF) {
                // Using can_recover() from the Zephyr driver seems obvious but
                // this didn't work reliably during testing More reliable:
                // restart the hole driver
                can_restart();
                return RET_ERROR_INTERNAL;
            }
        }

        if (transmission_complete_timeout_ms <= 0) {
            can_restart();
            return RET_ERROR_INTERNAL;
        }
    }

    return RET_SUCCESS;
}

static ret_code_t
request_battery_info(void)
{
    LOG_DBG("request battery info");

    ret_code_t ret;
    for (uint32_t id = 0x522; id <= 0x525; id++) {
        ret = send_rtr_message(id);
        if (ret != RET_SUCCESS) {
            return ret;
        }
    }

    return ret;
}

static void
message_checker(const struct device *dev, struct can_frame *frame,
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
check_battery_voltage()
{
    if (can_message_414_received) {
        uint32_t voltage_mv;

        CRITICAL_SECTION_ENTER(k);
        voltage_mv = state_414.voltage_group_1 + state_414.voltage_group_2 +
                     state_414.voltage_group_3 + state_414.voltage_group_4;
        CRITICAL_SECTION_EXIT(k);

        if (voltage_mv < BATTERY_MINIMUM_VOLTAGE_RUNTIME_MV) {
            // blink operator leds
            battery_low_operator_leds_blink();
            reboot(1);
        }
    }
}

static void
clear_can_message_buffers(void)
{
    CRITICAL_SECTION_ENTER(k);
    memset(&state_410, 0, sizeof(state_410));
    memset(&state_411, 0, sizeof(state_411));
    memset(&state_412, 0, sizeof(state_412));
    memset(&state_414, 0, sizeof(state_414));
    memset(&state_415, 0, sizeof(state_415));
    memset(&state_490, 0, sizeof(state_490));
    memset(&state_491, 0, sizeof(state_491));
    memset(&state_492, 0, sizeof(state_492));
    memset(&state_499, 0, sizeof(state_499));
    memset(&state_522, 0, sizeof(state_522));
    memset(&state_523, 0, sizeof(state_523));
    memset(&state_524, 0, sizeof(state_524));
    memset(&state_525, 0, sizeof(state_525));
    CRITICAL_SECTION_EXIT(k);

    can_message_410_received = false;
    can_message_411_received = false;
    can_message_412_received = false;
    can_message_414_received = false;
    can_message_415_received = false;
    can_message_499_received = false;
    can_message_414_received = false;
    can_message_522_received = false;
    can_message_523_received = false;
    can_message_524_received = false;
    can_message_525_received = false;
}

_Noreturn static void
battery_rx_thread()
{
    bool got_battery_voltage_can_message_local = false;
    uint32_t battery_messages_timeout = 0;

    bool battery_mcu_id_request_pending = false;

    while (1) {
        // We need to check if the CAN controller transitioned into bus-off
        // state.
        // When Batteries are swapped, sometimes too many CAN errors occur and
        // the controller changes into the bus-off state. Usually after a
        // Battery is swapped, the error counter is still low enough but after
        // the Battery MCU ID gets requested the counter increases over the
        // limit for going into bus-off. The Battery MCU ID request leads to
        // errors on the bus because the Battery MCU is usually in sleep mode
        // and needs some time to wake up before handling the request. During
        // this wakeup time, CAN bus errors occur since the RTR messages are not
        // acknowledged.
        if (current_can_controller_state_changed) {
            current_can_controller_state_changed = false;
            LOG_DBG("<!> can state changed to: %d",
                    current_can_controller_state);

            if (current_can_controller_state == CAN_STATE_BUS_OFF) {
                // Using can_recover() from the Zephyr driver seems obvious but
                // this didn't work reliably during testing More reliable:
                // restart the hole driver
                can_restart();
            }
        }

        check_battery_voltage();

        publish_battery_voltages();
        publish_battery_capacity();
        publish_battery_is_charging();
        publish_battery_cell_temperature();
        publish_battery_diagnostics();
        publish_battery_pcb_temperature();
        publish_battery_pack_temperature();

        if (publish_battery_info_request) {
            publish_battery_info_request = false;
            publish_battery_info();
        }

        if (can_message_400_received) {
            can_message_400_received = false;
            publish_battery_reset_reason();
        }

        if (can_message_522_received && can_message_523_received &&
            can_message_524_received && can_message_525_received) {
            // response for RTR messages 0x524 and 0x525 (MCU ID) received
            can_message_522_received = false;
            can_message_523_received = false;
            can_message_524_received = false;
            can_message_525_received = false;
            request_battery_info_left_attempts = 0;
            battery_mcu_id_request_pending = false;

            LOG_INF("Battery ID: 0x%08x%08x%08x",
                    state_525.battery_mcu_id_bit_95_64,
                    state_525.battery_mcu_id_bit_63_32,
                    state_524.battery_mcu_id_bit_31_0);

            // battery info will be published in next thread cycle
            // This gives some time to receive other CAN messages
            publish_battery_info_request = true;
        }

        // on older Battery firmware versions the MCU ID request won't be
        // answered by the Battery. In this case we publish the battery info
        // every 10 seconds.
        if (battery_mcu_id_request_pending &&
            (request_battery_info_left_attempts == 0)) {
            battery_mcu_id_request_pending = false;
            LOG_DBG("Battery MCU ID request was not answered. Probably EV1 or "
                    "EV2 battery was inserted. Transmit battery info without "
                    "MCU ID and firmware GIT hash.");

            // battery info will be published in next thread cycle
            publish_battery_info_request = true;
        }

        if (!dev_mode) {
            // check that we are still receiving messages from the battery
            // and consider the battery as removed if no message
            // has been received for BATTERY_MESSAGES_TIMEOUT_MS
            CRITICAL_SECTION_ENTER(k);
            got_battery_voltage_can_message_local = can_message_414_received;
            can_message_414_received = false;
            CRITICAL_SECTION_EXIT(k);

            if (got_battery_voltage_can_message_local) {
                // request battery info only if communication to the
                // Jetson is active
                if (request_battery_info_left_attempts &&
                    publish_is_started(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE)) {

                    // clear all can message buffers because they might contain
                    // data from the previously inserted battery
                    clear_can_message_buffers();
                    battery_mcu_id_request_pending = true;
                    request_battery_info();
                    request_battery_info_left_attempts--;
                }
                battery_messages_timeout = 0;
            } else {
                // no messages received from battery
                LOG_INF("Battery removed?");
                battery_messages_timeout += BATTERY_INFO_SEND_PERIOD_MS;
                request_battery_info_left_attempts =
                    BATTERY_ID_REQUEST_ATTEMPTS;
                if (battery_messages_timeout >= BATTERY_MESSAGES_TIMEOUT_MS) {
                    LOG_INF("No messages received from battery -> rebooting");
                    reboot(0);
                }
            }
        }

        k_msleep(BATTERY_INFO_SEND_PERIOD_MS);
    }
}

// called in interrupt context!
static void
can_state_change_callback(const struct device *dev, enum can_state state,
                          struct can_bus_err_cnt err_cnt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(err_cnt);
    ARG_UNUSED(user_data);
    current_can_controller_state_changed = true;
    current_can_controller_state = state;
}

ret_code_t
battery_init(void)
{
    int ret;

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN not ready");
        return RET_ERROR_INVALID_STATE;
    } else {
        LOG_INF("CAN ready");
    }

    ret = setup_filters();
    if (ret != RET_SUCCESS) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = can_start(can_dev);
    if (ret != RET_SUCCESS) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    can_set_state_change_callback(can_dev, can_state_change_callback, NULL);

    uint32_t full_voltage_mv = 0;
    uint32_t battery_cap_percentage = 0;
    bool got_battery_voltage_can_message_local;

    for (size_t i = 0; i < WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS /
                               WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS;
         ++i) {
        CRITICAL_SECTION_ENTER(k);
        full_voltage_mv = state_414.voltage_group_1 +
                          state_414.voltage_group_2 +
                          state_414.voltage_group_3 + state_414.voltage_group_4;
        battery_cap_percentage = state_499.state_of_charge;
        got_battery_voltage_can_message_local = can_message_414_received;
        CRITICAL_SECTION_EXIT(k);
        if (full_voltage_mv >= BATTERY_MINIMUM_VOLTAGE_STARTUP_MV &&
            battery_cap_percentage >=
                BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT) {
            break;
        }
        k_msleep(WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS);
    }

    LOG_INF("Voltage from battery: %umV", full_voltage_mv);
    LOG_INF("Capacity from battery: %u%%", battery_cap_percentage);

    if (!got_battery_voltage_can_message_local) {
        ret = voltage_measurement_get(CHANNEL_VBAT_SW, &full_voltage_mv);
        ASSERT_SOFT(ret);

        LOG_INF("Voltage from power supply / super caps: %umV",
                full_voltage_mv);

        if (full_voltage_mv >= BATTERY_MINIMUM_VOLTAGE_STARTUP_MV) {
            LOG_WRN("🧑‍💻 Power supply mode [dev mode]");
            dev_mode = true;

            // insert some fake values to keep orb-core happy
            state_414.voltage_group_1 = 4000;
            state_414.voltage_group_2 = 4000;
            state_414.voltage_group_3 = 4000;
            state_414.voltage_group_4 = 4000;
            state_499.state_of_charge = 100;
            can_message_414_received = true;
            can_message_499_received = true;

            battery_cap_percentage = 100;
        }
    }

    // if voltage low:
    // - show the user by blinking the operator LED in red
    // - reboot to allow for button startup again, hopefully with
    //   more charge
    if (full_voltage_mv < BATTERY_MINIMUM_VOLTAGE_STARTUP_MV ||
        battery_cap_percentage < BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT) {
        battery_low_operator_leds_blink();

        LOG_ERR_IMM("Low battery voltage, rebooting!");
        NVIC_SystemReset();
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
