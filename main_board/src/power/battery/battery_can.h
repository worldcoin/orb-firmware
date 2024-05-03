#pragma once

#include <zephyr/kernel.h>

#if defined(CONFIG_BOARD_PEARL_MAIN)
// Definition of CAN messages from battery.
// For details see
// https://www.notion.so/worldcoin/EVT-Battery-CAN-Interface-1ca6c18055034532b509df32f385be6f

__PACKED_STRUCT __may_alias battery_400_s { uint8_t reset_reason; };

__PACKED_STRUCT __may_alias battery_410_s
{
    uint16_t bq769_control_status;
    uint16_t battery_status;
    uint8_t fet_status;
    uint8_t balancer_state;
};

__PACKED_STRUCT __may_alias battery_411_s
{
    uint8_t safety_alert_a;
    uint8_t safety_status_a;
    uint8_t safety_alert_b;
    uint8_t safety_status_b;
    uint8_t safety_alert_c;
    uint8_t safety_status_c;
};

__PACKED_STRUCT __may_alias battery_412_s
{
    uint8_t permanent_fail_alert_a;
    uint8_t permanent_fail_status_a;
    uint8_t permanent_fail_alert_b;
    uint8_t permanent_fail_status_b;
    uint8_t permanent_fail_alert_c;
    uint8_t permanent_fail_status_c;
    uint8_t permanent_fail_alert_d;
    uint8_t permanent_fail_status_d;
};

__PACKED_STRUCT __may_alias battery_414_s
{
    int16_t voltage_group_1; // unit milli-volts
    int16_t voltage_group_2; // unit milli-volts
    int16_t voltage_group_3; // unit milli-volts
    int16_t voltage_group_4; // unit milli-volts
};

__PACKED_STRUCT battery_415_s
{
    int16_t current_ma; // positive if current flowing into battery, negative if
                        // it flows out of it
    int16_t cell_temperature; // unit 0.1ºC
};

__PACKED_STRUCT __may_alias battery_490_s
{
    uint16_t number_of_charges;
    uint16_t maximum_capacity_mah;
    uint16_t maximum_cell_temp_deg_by_10;
    uint16_t maximum_pcb_temp_deg_by_10;
};

__PACKED_STRUCT __may_alias battery_491_s
{
    uint16_t maximum_charge_current_ma;
    uint16_t maximum_discharge_current_ma;
    uint16_t number_of_written_flash_variables_15_0;
    uint8_t number_of_written_flash_variables_23_16;
    uint8_t detected_hardware_revision;
};

__PACKED_STRUCT __may_alias battery_492_s
{
    uint8_t soc_state;
    uint8_t soc_calibration_state;
    uint16_t total_number_of_button_presses_15_0;
    uint8_t total_number_of_button_presses_23_16;
    uint16_t number_of_insertions_15_0;
    uint8_t number_of_insertions_23_16;
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

__PACKED_STRUCT __may_alias battery_522_s
{
    uint8_t hardware_version;
    uint8_t firmware_version_main;
    uint8_t firmware_version_major;
    uint8_t firmware_version_minor;
};

__PACKED_STRUCT __may_alias battery_523_s { uint8_t git_hash[8]; };

__PACKED_STRUCT __may_alias battery_524_s { uint32_t battery_mcu_id_bit_31_0; };

__PACKED_STRUCT __may_alias battery_525_s
{
    uint32_t battery_mcu_id_bit_63_32;
    uint32_t battery_mcu_id_bit_95_64;
};
#endif
