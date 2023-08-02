#include "app_config.h"
#include "errors.h"
#include "mcu_messaging.pb.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "temperature/sensors/temperature.h"
#include "ui/operator_leds/operator_leds.h"
#include "utils.h"
#include <app_assert.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include "logs_can.h"
LOG_MODULE_REGISTER(battery, CONFIG_BATTERY_LOG_LEVEL);

static const struct device *can_dev = DEVICE_DT_GET(DT_ALIAS(battery_can_bus));
K_THREAD_STACK_DEFINE(can_battery_rx_thread_stack, THREAD_STACK_SIZE_BATTERY);
static struct k_thread rx_thread_data = {0};

static const struct adc_dt_spec adc_vbat_sw = ADC_DT_SPEC_GET(DT_PATH(vbat_sw));

// minimum voltage needed to boot the Orb during startup (in millivolts)
#define BATTERY_MINIMUM_VOLTAGE_STARTUP_MV       13750
#define BATTERY_MINIMUM_VOLTAGE_RUNTIME_MV       12500
#define BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT 5

// the time between sends of battery data to the Jetson
#define BATTERY_INFO_SEND_PERIOD_MS 1000
#define BATTERY_MESSAGES_TIMEOUT_MS (BATTERY_INFO_SEND_PERIOD_MS * 5)
static_assert(
    BATTERY_MESSAGES_TIMEOUT_MS > BATTERY_INFO_SEND_PERIOD_MS * 3,
    "Coarse timing resolution to check if battery is still sending messages");

#define WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS 2000
#define WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS 100

struct battery_can_msg {
    uint32_t can_id;
    uint8_t msg_len;
    void (*handler)(struct can_frame *frame);
};

static struct can_filter battery_can_filter = {
    .id = 0, .mask = CAN_STD_ID_MASK, .flags = CAN_FILTER_DATA};

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
static bool dev_mode = false;

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
handle_499(struct can_frame *frame)
{
    CRITICAL_SECTION_ENTER(k);
    state_499 = *(struct battery_499_s *)frame->data;
    CRITICAL_SECTION_EXIT(k);
}

static void
handle_414(struct can_frame *frame)
{
    CRITICAL_SECTION_ENTER(k);
    got_battery_voltage_can_message = true;
    state_414 = *(struct battery_414_s *)frame->data;
    CRITICAL_SECTION_EXIT(k);
}

static void
handle_415(struct can_frame *frame)
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

static void
battery_rx_thread()
{
    bool got_battery_voltage_can_message_local = false;
    uint32_t battery_messages_timeout = 0;

    while (1) {
        check_battery_voltage();

        publish_battery_voltages();
        publish_battery_capacity();
        publish_battery_is_charging();
        publish_battery_cell_temperature();
        publish_battery_diagnostic_flags();
        publish_battery_pcb_temperature();

        if (!dev_mode) {
            // check that we are still receiving messages from the battery
            // and consider the battery as removed if no message
            // has been received for BATTERY_MESSAGES_TIMEOUT_MS
            CRITICAL_SECTION_ENTER(k);
            got_battery_voltage_can_message_local =
                got_battery_voltage_can_message;
            got_battery_voltage_can_message = false;
            CRITICAL_SECTION_EXIT(k);

            if (got_battery_voltage_can_message_local) {
                battery_messages_timeout = 0;
            } else {
                battery_messages_timeout += BATTERY_INFO_SEND_PERIOD_MS;

                if (battery_messages_timeout >= BATTERY_MESSAGES_TIMEOUT_MS) {
                    reboot(0);
                }
            }
        }

        k_msleep(BATTERY_INFO_SEND_PERIOD_MS);
    }
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
        got_battery_voltage_can_message_local = got_battery_voltage_can_message;
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
        if (!device_is_ready(adc_vbat_sw.dev)) {
            LOG_ERR("ADC device not found");
        } else {
            // provide power to operational amplifiers to enable power supply
            // measurement circuitry
            struct gpio_dt_spec supply_meas_enable_spec = GPIO_DT_SPEC_GET(
                DT_PATH(zephyr_user), supply_voltages_meas_enable_gpios);
            ret = gpio_pin_configure_dt(&supply_meas_enable_spec, GPIO_OUTPUT);
            ASSERT_SOFT(ret);
            ret |= gpio_pin_set_dt(&supply_meas_enable_spec, 1);
            if (ret) {
                LOG_ERR("IO error %d", ret);
                return RET_ERROR_INVALID_STATE;
            }

            // give some time to obtain battery voltage at the MCU pin
            k_msleep(1);

            // let's configure the ADC and ADC measurement
            struct adc_channel_cfg channel_cfg = {
                .channel_id = adc_vbat_sw.channel_id,
                .gain = ADC_GAIN_1,
                .reference = ADC_REF_INTERNAL,
                .acquisition_time = ADC_ACQ_TIME_DEFAULT,
                .differential = false,
            };
            adc_channel_setup(adc_vbat_sw.dev, &channel_cfg);

            int32_t vref_mv = adc_ref_internal(adc_vbat_sw.dev);
            int16_t sample_buffer = 0;
            struct adc_sequence sequence = {
                .channels = BIT(adc_vbat_sw.channel_id),
                .resolution = 12,
                .oversampling = 0,
                .buffer = &sample_buffer,
                .buffer_size = sizeof(sample_buffer),
            };

            // read sample
            ret = adc_read(adc_vbat_sw.dev, &sequence);
            if (ret != 0) {
                LOG_ERR("ADC ret %d", ret);
            } else {
                int32_t sample_i32 = sample_buffer;
                ret = adc_raw_to_millivolts(vref_mv, channel_cfg.gain,
                                            sequence.resolution, &sample_i32);
                // find voltage before divider bridge (R1=442k, R2=100k)
                sample_i32 = (int32_t)((float)sample_i32 * 5.42f);

                if (ret == 0) {
                    full_voltage_mv = sample_i32;
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

                        battery_cap_percentage = 100;
                    }
                }
            }

            // disable measurement circuitry
            ret = gpio_pin_configure_dt(&supply_meas_enable_spec,
                                        GPIO_DISCONNECTED);
            ASSERT_SOFT(ret);
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
