#include "app_config.h"
#include "battery.h"
#include "errors.h"
#include "mcu.pb.h"
#include "orb_state.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "temperature/sensors/temperature.h"
#include "ui/rgb_leds/operator_leds/operator_leds.h"
#include "utils.h"
#include "voltage_measurement/voltage_measurement.h"
#include <app_assert.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#if defined(CONFIG_MEMFAULT)
#include <memfault/core/reboot_tracking.h>
#if defined(CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE)
#include <memfault/metrics/battery.h>
#include <memfault/metrics/platform/battery.h>
#endif
#endif

#include "orb_logs.h"

LOG_MODULE_REGISTER(battery, CONFIG_BATTERY_LOG_LEVEL);
ORB_STATE_REGISTER(pwr_supply);

// English term `corded` applies to power supplies while `wired` is
// more for device connection (network)
static bool corded_power_supply = false;

K_THREAD_STACK_DEFINE(battery_rx_thread_stack, THREAD_STACK_SIZE_BATTERY);
static struct k_thread rx_thread_data = {0};

static const struct i2c_dt_spec i2c_device_spec =
    I2C_DT_SPEC_GET(DT_NODELABEL(bq4050));

static orb_mcu_main_BatteryCapacity battery_cap;
static orb_mcu_main_BatteryIsCharging is_charging;

#define BATTERY_INFO_SEND_PERIOD_MS         1000
#define BATTERY_MESSAGES_REMOVED_TIMEOUT_MS (BATTERY_INFO_SEND_PERIOD_MS * 3)
#define BATTERY_MESSAGES_FORCE_REBOOT_TIMEOUT_MS                               \
    (BATTERY_INFO_SEND_PERIOD_MS * 10)
BUILD_ASSERT(
    BATTERY_MESSAGES_FORCE_REBOOT_TIMEOUT_MS > BATTERY_INFO_SEND_PERIOD_MS * 3,
    "Coarse timing resolution to check if battery is still sending messages");

#define WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS 2000
#define WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS 100

#define BQ4050_CMD_CURRENT                   0x0A
#define BQ4050_CMD_RELATIVE_STATE_OF_CHARGE  0x0D
#define BQ4050_CMD_FULL_CHARGE_CAPACITY      0x10
#define BQ4050_CMD_CYCLE_COUNT               0x17
#define BQ4050_CMD_SERIAL_NUMBER             0x1C
#define BQ4050_CMD_MANUFACTURER_BLOCK_ACCESS 0x44
#define BQ4050_CMD_STATE_OF_HEALTH           0x4F

#define BQ4050_BLK_CMD_FIRMWARE_VERSION  0x0002
#define BQ4050_BLK_CMD_LIFETIME_DATA_1   0x0060
#define BQ4050_BLK_CMD_MANUFACTURER_INFO 0x0070
#define BQ4050_BLK_CMD_DA_STATUS_1       0x0071
#define BQ4050_BLK_CMD_DA_STATUS_2       0x0072

typedef struct {
    uint16_t cell_1_max_voltage_mv;
    uint16_t cell_2_max_voltage_mv;
    uint16_t cell_3_max_voltage_mv;
    uint16_t cell_4_max_voltage_mv;
    uint16_t cell_1_min_voltage_mv;
    uint16_t cell_2_min_voltage_mv;
    uint16_t cell_3_min_voltage_mv;
    uint16_t cell_4_min_voltage_mv;
    uint16_t max_delta_cell_voltage_mv;
    uint16_t max_charge_current_ma;
    int16_t max_discharge_current_ma;
    int16_t max_avg_dsg_current_ma;
    int16_t max_avg_dsg_power;
    int8_t max_temp_cell_degrees;
    int8_t min_temp_cell_degrees;
    int8_t max_delta_cell_temp_degrees;
    int8_t max_temp_int_sensor_degrees;
    int8_t min_temp_int_sensor_degrees;
    int8_t max_temp_fet_degrees;
} bq4050_lifetime_data_1_block_t;

typedef struct {
    uint16_t cell_voltage_1_mv;
    uint16_t cell_voltage_2_mv;
    uint16_t cell_voltage_3_mv;
    uint16_t cell_voltage_4_mv;
    // uncomment the following lines if these values are needed
    //    uint16_t bat_voltage_mv;
    //    uint16_t pack_voltage_mv;
    //    int16_t cell_current_1_ma;
    //    int16_t cell_current_2_ma;
    //    int16_t cell_current_3_ma;
    //    int16_t cell_current_4_ma;
} bq4050_da_status_1_block_t;

typedef struct {
    int16_t temperature_int_decikelvin;
    int16_t temperature_ts1_decikelvin;
    int16_t temperature_ts2_decikelvin;
    int16_t temperature_ts3_decikelvin;
    int16_t temperature_ts4_decikelvin;
    int16_t temperature_cell_decikelvin;
    int16_t temperature_fet_decikelvin;
} bq4050_da_status_2_block_t;

static int
bq4050_read_block(uint16_t command, uint8_t *data, uint16_t size)
{
    uint8_t tx_data[4];
    tx_data[0] = BQ4050_CMD_MANUFACTURER_BLOCK_ACCESS;
    tx_data[1] = 2;                       // Length header
    tx_data[2] = (uint8_t)command;        // Little endian
    tx_data[3] = (uint8_t)(command >> 8); // Little endian

    int ret = i2c_write_dt(&i2c_device_spec, tx_data, sizeof(tx_data));

    if (ret != 0) {
        return ret;
    }

    uint8_t man_block_access_command = BQ4050_CMD_MANUFACTURER_BLOCK_ACCESS;
    uint8_t rx_data[size +
                    3]; // + 1 for length header, + 2 for sending back command

    ret = i2c_write_read_dt(&i2c_device_spec, &man_block_access_command,
                            sizeof(man_block_access_command), rx_data,
                            sizeof(rx_data));

    if (ret != 0) {
        return ret;
    }

    memcpy(data, &rx_data[3], size);

    return 0;
}

static int
bq4050_read_word(uint8_t command, uint16_t *word)
{
    uint8_t rx_data[2];
    int ret = i2c_write_read_dt(&i2c_device_spec, &command, sizeof(command),
                                rx_data, sizeof(rx_data));

    if (ret != 0) {
        return ret;
    }

    *word = (((uint16_t)rx_data[1]) << 8) | ((uint16_t)rx_data[0]);

    return 0;
}

static ret_code_t
bq4050_read_firmware_build_number(uint16_t *build_number)
{
    if (build_number == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint8_t data[6];
    int ret =
        bq4050_read_block(BQ4050_BLK_CMD_FIRMWARE_VERSION, data, sizeof(data));

    if (ret == 0) {
        *build_number = (((uint16_t)data[5]) << 8) | ((uint16_t)data[4]);
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_cycle_count(uint16_t *cycle_count)
{
    if (cycle_count == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_CYCLE_COUNT, &word);

    if (ret == 0) {
        *cycle_count = word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_current(int16_t *current_ma)
{
    if (current_ma == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_CURRENT, &word);

    if (ret == 0) {
        *current_ma = (int16_t)word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_relative_state_of_charge(uint8_t *percentage)
{
    if (percentage == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_RELATIVE_STATE_OF_CHARGE, &word);

    if (ret == 0) {
        *percentage = (uint8_t)word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_full_charge_capacity(uint16_t *full_charge_capacity_mah)
{
    if (full_charge_capacity_mah == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_FULL_CHARGE_CAPACITY, &word);

    if (ret == 0) {
        *full_charge_capacity_mah = word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_serial_number(uint16_t *serial_number)
{
    if (serial_number == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_SERIAL_NUMBER, &word);

    if (ret == 0) {
        *serial_number = word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static ret_code_t
bq4050_read_state_of_health(uint8_t *state_of_health_percentage)
{
    if (state_of_health_percentage == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    uint16_t word = 0;
    int ret = bq4050_read_word(BQ4050_CMD_STATE_OF_HEALTH, &word);

    if (ret == 0) {
        *state_of_health_percentage = (uint8_t)word;
        return RET_SUCCESS;
    } else {
        return RET_ERROR_INTERNAL;
    }
}

static void
publish_battery_voltages(orb_mcu_main_BatteryVoltage *voltages)
{
    LOG_DBG("Battery voltage: (%d, %d, %d, %d) mV", voltages->battery_cell1_mv,
            voltages->battery_cell2_mv, voltages->battery_cell3_mv,
            voltages->battery_cell4_mv);
    publish_new(voltages, sizeof(orb_mcu_main_BatteryVoltage),
                orb_mcu_main_McuToJetson_battery_voltage_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_capacity(orb_mcu_main_BatteryCapacity *battery_cap)
{
    LOG_DBG("State of charge: %u%%", battery_cap->percentage);
    publish_new(battery_cap, sizeof(orb_mcu_main_BatteryCapacity),
                orb_mcu_main_McuToJetson_battery_capacity_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_is_charging(orb_mcu_main_BatteryIsCharging *is_charging)
{
    LOG_DBG("Is charging? %s", is_charging->battery_is_charging ? "yes" : "no");
    publish_new(is_charging, sizeof(orb_mcu_main_BatteryIsCharging),
                orb_mcu_main_McuToJetson_battery_is_charging_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_cell_temperature(int16_t cell_temperature_decidegrees)
{
    LOG_DBG("Battery cell temperature: %u.%u°C",
            cell_temperature_decidegrees / 10,
            cell_temperature_decidegrees % 10);
    temperature_report(orb_mcu_Temperature_TemperatureSource_BATTERY_CELL,
                       cell_temperature_decidegrees / 10);
}

static void
publish_battery_diagnostics_common(
    orb_mcu_main_BatteryDiagnosticCommon *diag_common)
{
    LOG_DBG("Publishing battery diagnostics common");
    publish_new(diag_common, sizeof(orb_mcu_main_BatteryDiagnosticCommon),
                orb_mcu_main_McuToJetson_battery_diag_common_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
publish_battery_pcb_temperature(int16_t pcb_temperature_decidegrees)
{
    LOG_DBG("Battery PCB temperature: %u.%u°C",
            pcb_temperature_decidegrees / 10, pcb_temperature_decidegrees % 10);
    temperature_report(orb_mcu_Temperature_TemperatureSource_BATTERY_PCB,
                       pcb_temperature_decidegrees / 10);
}

static void
check_battery_voltage(uint16_t battery_voltage_mv)
{
    if (battery_voltage_mv < BATTERY_MINIMUM_VOLTAGE_RUNTIME_MV) {
        operator_leds_indicate_low_battery_blocking();
        reboot(1);
    }
}

_Noreturn static void
battery_rx_thread()
{
    uint32_t battery_messages_timeout = 0;
    int shutdown_schuduled_sent = RET_ERROR_NOT_INITIALIZED;

    while (1) {
        bool got_battery_voltage_message_local = false;
        int ret;

        // The following sections are placed in {}-scopes because then the
        // memory of the local variables can be reused by the subsequent code.
        {
            bq4050_da_status_1_block_t da_status_1 = {0};
            ret =
                bq4050_read_block(BQ4050_BLK_CMD_DA_STATUS_1,
                                  (uint8_t *)&da_status_1, sizeof(da_status_1));
            if (ret == 0) {
                got_battery_voltage_message_local = true;

                orb_mcu_main_BatteryVoltage voltages;
                voltages.battery_cell1_mv = da_status_1.cell_voltage_1_mv;
                voltages.battery_cell2_mv = da_status_1.cell_voltage_2_mv;
                voltages.battery_cell3_mv = da_status_1.cell_voltage_3_mv;
                voltages.battery_cell4_mv = da_status_1.cell_voltage_4_mv;

                uint32_t pack_voltage_mv =
                    voltages.battery_cell1_mv + voltages.battery_cell2_mv +
                    voltages.battery_cell3_mv + voltages.battery_cell4_mv;
                check_battery_voltage(pack_voltage_mv);

                publish_battery_voltages(&voltages);
            } else if (corded_power_supply) {
                // send fake values to keep orb-core happy
                orb_mcu_main_BatteryVoltage voltages;
                voltages.battery_cell1_mv = 4000;
                voltages.battery_cell2_mv = 4000;
                voltages.battery_cell3_mv = 4000;
                voltages.battery_cell4_mv = 4000;

                publish_battery_voltages(&voltages);
            }
        }

        {
            uint8_t relative_soc = 0;
            ret = bq4050_read_relative_state_of_charge(&relative_soc);

            if (ret == 0) {
                if (battery_cap.percentage != relative_soc) {
                    LOG_INF("Main battery: %u%%", relative_soc);
                }

                battery_cap.percentage = relative_soc;
                publish_battery_capacity(&battery_cap);
            } else if (corded_power_supply) {
                // send fake values to keep orb-core happy
                battery_cap.percentage = 100;

                publish_battery_capacity(&battery_cap);
            }
        }

        {
            bq4050_da_status_2_block_t da_status_2 = {0};
            ret =
                bq4050_read_block(BQ4050_BLK_CMD_DA_STATUS_2,
                                  (uint8_t *)&da_status_2, sizeof(da_status_2));

            if (ret == 0) {
                const int16_t kelvin_offset_decidegrees = -2732;
                publish_battery_pcb_temperature(
                    da_status_2.temperature_ts2_decikelvin +
                    kelvin_offset_decidegrees);
                publish_battery_cell_temperature(
                    da_status_2.temperature_ts3_decikelvin +
                    kelvin_offset_decidegrees);
            }
        }

        {
            if (corded_power_supply) {
                is_charging.battery_is_charging = true;
            } else {
                int16_t current_ma;
                ret = bq4050_read_current(&current_ma);

                if (ret == 0) {
                    LOG_DBG("Battery current: %d mA", current_ma);

                    orb_mcu_main_BatteryDiagnosticCommon diag_common = {0};
                    diag_common.current_ma = current_ma;
                    publish_battery_diagnostics_common(&diag_common);

                    if (is_charging.battery_is_charging != (current_ma > 0)) {
                        LOG_INF("Is charging: %s",
                                (current_ma > 0) ? "yes" : "no");

#ifdef CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE
                        if (current_ma > 0) {
                            memfault_metrics_battery_stopped_discharging();
                        }
#endif
                    }

                    is_charging.battery_is_charging = (current_ma > 0);

                    publish_battery_is_charging(&is_charging);
                }
            }
        }

        {
            orb_mcu_main_BatteryInfoHwFw info_hw_fw = {0};
            info_hw_fw.mcu_id.size = 12; // set mcu_id size to 12 bytes,
            // otherwise orb-core will panic

            uint16_t serial_number = 0xFFFF;
            ret = bq4050_read_serial_number(&serial_number);
            if (ret == RET_SUCCESS) {
                LOG_DBG("Serial number: 0x%04X", serial_number);
            }

            info_hw_fw.mcu_id.bytes[11] = serial_number;
            info_hw_fw.mcu_id.bytes[10] = serial_number >> 8;

            info_hw_fw.hw_version =
                orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_UNDETECTED;

            uint8_t manufacturer_info[33] = {
                0}; // size is one byte bigger than the field in the BQ4050 to
                    // make sure the string is terminated
            const uint8_t pcb_version_r00[] = "IDU139GA-R00";
            ret = bq4050_read_block(BQ4050_BLK_CMD_MANUFACTURER_INFO,
                                    manufacturer_info,
                                    sizeof(manufacturer_info) - 1);
            if (ret == RET_SUCCESS) {
                LOG_DBG("Manufacturer info: %s", manufacturer_info);

#define MANUFACTURER_INFO_COMMON_STR_SIZE 11
                bool pcb_version_string_match =
                    (memcmp(manufacturer_info, pcb_version_r00,
                            MANUFACTURER_INFO_COMMON_STR_SIZE) == 0);

                if (pcb_version_string_match) {
                    switch (manufacturer_info[11]) {
                    case '0':
                        info_hw_fw.hw_version =
                            orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_IDU139GA_R00;
                        break;
                    case '1':
                        info_hw_fw.hw_version =
                            orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_IDU139GA_R01;
                        break;
                    case '2':
                        info_hw_fw.hw_version =
                            orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_IDU139GA_R02;
                        break;
                    case '3':
                        info_hw_fw.hw_version =
                            orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_IDU139GA_R03;
                        break;
                    default:
                        info_hw_fw.hw_version =
                            orb_mcu_main_BatteryInfoHwFw_HardwareVersion_BATTERY_HW_VERSION_UNDETECTED;
                        break;
                    }
                }
            }

            uint16_t build_number;
            ret = bq4050_read_firmware_build_number(&build_number);
            if (ret == RET_SUCCESS) {
                LOG_DBG("FW build number: 0x%04X", build_number);
                info_hw_fw.fw_version.major = build_number;
                info_hw_fw.has_fw_version = true;

                publish_new(&info_hw_fw, sizeof(info_hw_fw),
                            orb_mcu_main_McuToJetson_battery_info_hw_fw_tag,
                            CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            }
        }

        {
            uint16_t cycle_count;
            ret = bq4050_read_cycle_count(&cycle_count);
            if (ret == RET_SUCCESS) {
                LOG_DBG("Cycle count: %d", cycle_count);

                orb_mcu_main_BatteryInfoSocAndStatistics
                    info_soc_and_statistics = {0};
                info_soc_and_statistics.number_of_charges = cycle_count;

                publish_new(
                    &info_soc_and_statistics, sizeof(info_soc_and_statistics),
                    orb_mcu_main_McuToJetson_battery_info_soc_and_statistics_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            }
        }

        {
            orb_mcu_main_BatteryInfoMaxValues info_max_values = {0};
            uint16_t full_charge_capacity_mah;
            ret = bq4050_read_full_charge_capacity(&full_charge_capacity_mah);
            if (ret == RET_SUCCESS) {
                LOG_DBG("Full charge capacity: %d mA",
                        full_charge_capacity_mah);

                info_max_values.maximum_capacity_mah = full_charge_capacity_mah;
            }

            bq4050_lifetime_data_1_block_t lifetime_data_1 = {0};
            ret = bq4050_read_block(BQ4050_BLK_CMD_LIFETIME_DATA_1,
                                    (uint8_t *)&lifetime_data_1,
                                    sizeof(lifetime_data_1));
            if (ret == 0) {
                LOG_DBG(
                    "Max values - cha curr: %d mA, discha curr: %d mA, cell "
                    "temp: %d dC, fet temp: %d dC",
                    lifetime_data_1.max_charge_current_ma,
                    lifetime_data_1.max_discharge_current_ma,
                    lifetime_data_1.max_temp_cell_degrees,
                    lifetime_data_1.max_temp_fet_degrees);

                info_max_values.maximum_cell_temp_decidegrees =
                    (uint16_t)lifetime_data_1.max_temp_cell_degrees * 10;
                info_max_values.maximum_pcb_temp_decidegrees =
                    (uint16_t)lifetime_data_1.max_temp_fet_degrees * 10;
                info_max_values.maximum_charge_current_ma =
                    lifetime_data_1.max_charge_current_ma;

                if (lifetime_data_1.max_discharge_current_ma > 0) {
                    LOG_WRN("max_discharge_current_ma = %d > 0",
                            lifetime_data_1.max_discharge_current_ma);
                    info_max_values.maximum_discharge_current_ma = 0;
                } else {
                    info_max_values.maximum_discharge_current_ma =
                        -lifetime_data_1.max_discharge_current_ma;
                }

                publish_new(
                    &info_max_values, sizeof(info_max_values),
                    orb_mcu_main_McuToJetson_battery_info_max_values_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            }
        }

        {
            uint8_t state_of_health_percentage;
            ret = bq4050_read_state_of_health(&state_of_health_percentage);

            if (ret == 0) {
                LOG_DBG("Battery SoH: %d %%", state_of_health_percentage);

                orb_mcu_main_BatteryStateOfHealth state_of_health = {0};
                state_of_health.percentage = state_of_health_percentage;

                publish_new(
                    &state_of_health, sizeof(state_of_health),
                    orb_mcu_main_McuToJetson_battery_state_of_health_tag,
                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
            }
        }

        if (!corded_power_supply) {
            // check that we are still receiving messages from the battery
            // and consider the battery as removed if no message
            // has been received for BATTERY_MESSAGES_TIMEOUT_MS

            if (got_battery_voltage_message_local) {
                if (battery_messages_timeout != 0) {
                    ORB_STATE_SET_CURRENT(RET_SUCCESS, "battery comm ok");
                }
                battery_messages_timeout = 0;
                shutdown_schuduled_sent = RET_ERROR_NOT_INITIALIZED;
            } else {
                // no messages received from the battery
                if (battery_messages_timeout == 0) {
                    ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE,
                                          "battery link lost, not inserted?");
                }
                battery_messages_timeout += BATTERY_INFO_SEND_PERIOD_MS;

                // consider battery removed after
                // BATTERY_MESSAGES_REMOVED_TIMEOUT_MS
                if (battery_messages_timeout >=
                        BATTERY_MESSAGES_REMOVED_TIMEOUT_MS &&
                    shutdown_schuduled_sent != RET_SUCCESS) {
                    orb_mcu_main_ShutdownScheduled shutdown;
                    shutdown.shutdown_reason =
                        orb_mcu_main_ShutdownScheduled_ShutdownReason_BATTERY_REMOVED;
                    shutdown.has_ms_until_shutdown = true;
                    shutdown.ms_until_shutdown =
                        (BATTERY_MESSAGES_FORCE_REBOOT_TIMEOUT_MS -
                         battery_messages_timeout);
                    shutdown_schuduled_sent =
                        publish_new(&shutdown, sizeof(shutdown),
                                    orb_mcu_main_McuToJetson_shutdown_tag,
                                    CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
                    LOG_WRN("Battery removed: %d", shutdown_schuduled_sent);
                }
                if (battery_messages_timeout >=
                    BATTERY_MESSAGES_FORCE_REBOOT_TIMEOUT_MS) {
                    LOG_INF("No messages received from battery -> rebooting");
#ifdef CONFIG_MEMFAULT
                    MEMFAULT_REBOOT_MARK_RESET_IMMINENT(
                        kMfltRebootReason_BatteryRemoved);
#endif
                    reboot(0);
                }
            }
        }

        k_msleep(BATTERY_INFO_SEND_PERIOD_MS);
    }
}

#ifdef CONFIG_SHELL

/** Dump as many battery stats as possible over printk **/
void
battery_dump_stats(const struct shell *sh)
{
    uint16_t serial_number = 0;
    ret_code_t ret = bq4050_read_serial_number(&serial_number);
    if (ret == RET_SUCCESS) {
        shell_print(sh, "Serial number: 0x%04X", serial_number);
    } else {
        shell_print(sh, "Failed to read serial number: %d", ret);
    }

    // current voltages
    bq4050_da_status_1_block_t da_status_1 = {0};
    ret = bq4050_read_block(BQ4050_BLK_CMD_DA_STATUS_1, (uint8_t *)&da_status_1,
                            sizeof(da_status_1));
    if (ret == RET_SUCCESS) {
        shell_print(
            sh, "Cell voltages: %d mV, %d mV, %d mV, %d mV",
            da_status_1.cell_voltage_1_mv, da_status_1.cell_voltage_2_mv,
            da_status_1.cell_voltage_3_mv, da_status_1.cell_voltage_4_mv);
    } else {
        shell_print(sh, "Failed to read cell voltages: %d", ret);
    }

    uint8_t relative_soc = 0;
    ret = bq4050_read_relative_state_of_charge(&relative_soc);
    if (ret == RET_SUCCESS) {
        shell_print(sh, "Relative state of charge: %d%%", relative_soc);
    } else {
        shell_print(sh, "Failed to read relative state of charge: %d", ret);
    }

    int16_t current_ma = 0;
    ret = bq4050_read_current(&current_ma);
    if (ret == RET_SUCCESS) {
        shell_print(sh, "Current: %d mA", current_ma);
    } else {
        shell_print(sh, "Failed to read current: %d", ret);
    }
    uint16_t full_charge_capacity_mah = 0;
    ret = bq4050_read_full_charge_capacity(&full_charge_capacity_mah);
    if (ret == RET_SUCCESS) {
        shell_print(sh, "Full charge capacity: %d mAh",
                    full_charge_capacity_mah);
    } else {
        shell_print(sh, "Failed to read full charge capacity: %d", ret);
    }

    uint16_t cycle_count = 0;
    ret = bq4050_read_cycle_count(&cycle_count);
    if (ret == RET_SUCCESS) {
        shell_print(sh, "Cycle count: %d", cycle_count);
    } else {
        shell_print(sh, "Failed to read cycle count: %d", ret);
    }
}

#endif

ret_code_t
battery_init(void)
{
    int ret;

    if (!device_is_ready(i2c_device_spec.bus)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INTERNAL;
    }

    uint32_t full_voltage_mv = 0;
    uint8_t battery_cap_percentage = 0;
    bool battery_voltage_message_received = false;

    for (size_t i = 0; i < WAIT_FOR_VOLTAGES_TOTAL_PERIOD_MS /
                               WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS;
         ++i) {
        bq4050_da_status_1_block_t da_status_1 = {0};
        ret = bq4050_read_block(BQ4050_BLK_CMD_DA_STATUS_1,
                                (uint8_t *)&da_status_1, sizeof(da_status_1));
        if (ret == 0) {
            battery_voltage_message_received = true;
        }

        full_voltage_mv =
            da_status_1.cell_voltage_1_mv + da_status_1.cell_voltage_2_mv +
            da_status_1.cell_voltage_3_mv + da_status_1.cell_voltage_4_mv;
        bq4050_read_relative_state_of_charge(&battery_cap_percentage);

        if (full_voltage_mv >= BATTERY_MINIMUM_VOLTAGE_STARTUP_MV &&
            battery_cap_percentage >=
                BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT) {
            break;
        }
        k_msleep(WAIT_FOR_VOLTAGES_CHECK_PERIOD_MS);
    }

    LOG_INF("Voltage from battery: %umV", full_voltage_mv);
    LOG_INF("Capacity from battery: %u%%", battery_cap_percentage);

    if (!battery_voltage_message_received) {
        ret = voltage_measurement_get(CHANNEL_VBAT_SW, &full_voltage_mv);
        ASSERT_SOFT(ret);

        LOG_INF("Voltage from power supply / super caps: %umV",
                full_voltage_mv);

        if (full_voltage_mv >= BATTERY_MINIMUM_VOLTAGE_STARTUP_MV) {
            LOG_INF("🔌 Corded power supply mode");
            corded_power_supply = true;
            ORB_STATE_SET_CURRENT(RET_SUCCESS, "corded");

            battery_cap_percentage = 100;
        }
    }

    // if voltage low:
    // - show the user by blinking the operator LED in red
    // - reboot to allow for button startup again, hopefully with
    //   more charge
    if (full_voltage_mv < BATTERY_MINIMUM_VOLTAGE_STARTUP_MV ||
        battery_cap_percentage < BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT) {
        LOG_ERR_IMM("Low battery voltage, rebooting!");
        operator_leds_indicate_low_battery_blocking();

#ifdef CONFIG_MEMFAULT
        MEMFAULT_REBOOT_MARK_RESET_IMMINENT(kMfltRebootReason_LowPower);
#endif
        NVIC_SystemReset();
        CODE_UNREACHABLE;
    }

    LOG_INF("Battery voltage is ok");
    ORB_STATE_SET_CURRENT(RET_SUCCESS, "battery comm ok");

    k_tid_t tid =
        k_thread_create(&rx_thread_data, battery_rx_thread_stack,
                        K_THREAD_STACK_SIZEOF(battery_rx_thread_stack),
                        (k_thread_entry_t)battery_rx_thread, NULL, NULL, NULL,
                        THREAD_PRIORITY_BATTERY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "battery");

    return RET_SUCCESS;
}

#if defined(CONFIG_MEMFAULT_METRICS_BATTERY_ENABLE)

// This function is called by the Memfault SDK at each Heartbeat interval end,
// to get the current battery state-of-charge and discharging state.
int
memfault_platform_get_stateofcharge(sMfltPlatformBatterySoc *soc)
{
    CRITICAL_SECTION_ENTER(k);

    *soc = (sMfltPlatformBatterySoc){
        .soc = battery_cap.percentage,
        .discharging = (is_charging.battery_is_charging == false),
    };

    CRITICAL_SECTION_EXIT(k);
    return 0;
}

#endif
