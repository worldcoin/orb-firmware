#include "version.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <dfu.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(version, CONFIG_VERSION_LOG_LEVEL);

/**
 * # Pearl Orbs
 * Hardware version can be fetched using UC_ADC_HW_VERSION on the main board:
 * - 3.0 firmware is specific, so we can provide an hardcoded implementation
 * - v3.1 pull down
 * - v3.2 pull up
 * GPIO logic level can then be used to get the hardware version
 *
 * # Diamond Orbs
 * ## Main board
 * Hardware version can be fetched using IO expander on the main board:
 * - v4.0 p[13..10] = 0
 * - v4.1 p[13..10] = 1
 * - v4.2 p[13..10] = 2
 *
 * ## Front unit
 * Hardware version can be fetched using IO expander on the front unit:
 * - v6.0 p[13..10] = 0
 * - v6.1 p[13..10] = 1
 * - v6.2A p[13..10] = 2
 * - v6.2B p[13..10] = 3
 *
 * ## Power board
 * Hardware version can be fetched using IO expander on the power board:
 * - v1.0: p[13..10] = 0
 * - v1.1: p[13..10] = 1
 * - v1.2: p[13..10] = 2
 **/

#if defined(CONFIG_BOARD_PEARL_MAIN)
const struct adc_dt_spec adc_dt_spec = ADC_DT_SPEC_GET(DT_PATH(board_version));

#define ADC_RESOLUTION_BITS  12
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
static const struct gpio_dt_spec hw_main_board_bit0 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_main_board_gpios, 0);
static const struct gpio_dt_spec hw_main_board_bit1 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_main_board_gpios, 1);
static const struct gpio_dt_spec hw_main_board_bit2 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_main_board_gpios, 2);
static const struct gpio_dt_spec hw_main_board_bit3 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_main_board_gpios, 3);

static const struct gpio_dt_spec hw_front_unit_bit0 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_front_unit_gpios, 0);
static const struct gpio_dt_spec hw_front_unit_bit1 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_front_unit_gpios, 1);
static const struct gpio_dt_spec hw_front_unit_bit2 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_front_unit_gpios, 2);
static const struct gpio_dt_spec hw_front_unit_bit3 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_front_unit_gpios, 3);

static const struct gpio_dt_spec hw_pwr_board_bit0 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_pwr_board_gpios, 0);
static const struct gpio_dt_spec hw_pwr_board_bit1 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_pwr_board_gpios, 1);
static const struct gpio_dt_spec hw_pwr_board_bit2 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_pwr_board_gpios, 2);
static const struct gpio_dt_spec hw_pwr_board_bit3 = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), hw_version_pwr_board_gpios, 3);

#endif

static Hardware_OrbVersion version = Hardware_OrbVersion_HW_VERSION_UNKNOWN;

int
version_fw_send(uint32_t remote)
{
    struct image_version version = {0};

    dfu_version_primary_get(&version);

    Versions versions = {
        .has_primary_app = true,
        .primary_app.major = version.iv_major,
        .primary_app.minor = version.iv_minor,
        .primary_app.patch = version.iv_revision,
        .primary_app.commit_hash = version.iv_build_num,
    };

    memset(&version, 0, sizeof version);
    if (dfu_version_secondary_get(&version) == 0) {
        versions.has_secondary_app = true;
        versions.secondary_app.major = version.iv_major;
        versions.secondary_app.minor = version.iv_minor;
        versions.secondary_app.patch = version.iv_revision;
        versions.secondary_app.commit_hash = version.iv_build_num;
    }

    return publish_new(&versions, sizeof(versions), McuToJetson_versions_tag,
                       remote);
}

static ret_code_t
version_fetch_hardware_rev(Hardware *hw_version)
{
    // read only once and keep hardware version into `version`
    if (version == Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
#if defined(CONFIG_BOARD_PEARL_MAIN)
        if (!device_is_ready(adc_dt_spec.dev)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            return RET_ERROR_INVALID_STATE;
        }

        int vref_mv = adc_ref_internal(adc_dt_spec.dev);

        // ADC config
        struct adc_channel_cfg channel_cfg = {
            .channel_id = adc_dt_spec.channel_id,
            .gain = ADC_GAIN,
            .reference = ADC_REFERENCE,
            .acquisition_time = ADC_ACQUISITION_TIME,
        };
        adc_channel_setup(adc_dt_spec.dev, &channel_cfg);

        int16_t sample_buffer = 0;
        struct adc_sequence sequence = {
            .buffer = &sample_buffer,
            .buffer_size = sizeof(sample_buffer),
            .channels = BIT(adc_dt_spec.channel_id),
            .resolution = ADC_RESOLUTION_BITS,
            .oversampling = 0,
            .options = NULL,
        };

        int err_code = adc_read(adc_dt_spec.dev, &sequence);
        if (err_code < 0) {
            return RET_ERROR_INTERNAL;
        } else {
            int32_t hardware_version_mv = sample_buffer;
            adc_raw_to_millivolts(vref_mv, ADC_GAIN_1, ADC_RESOLUTION_BITS,
                                  &hardware_version_mv);

            LOG_DBG("Hardware rev voltage: %dmV", hardware_version_mv);

            if (hardware_version_mv > 3200) {
                // should be 3.3V = 3300mV (Mainboard 3.2)
                version = Hardware_OrbVersion_HW_VERSION_PEARL_EV2;
            } else if (hardware_version_mv > 2900) {
                // should be 3.0V = 3000mV (Mainboard 3.3)
                version = Hardware_OrbVersion_HW_VERSION_PEARL_EV3;
            } else if (hardware_version_mv < 100) {
                // should be 0.0V (Mainboard 3.1)
                version = Hardware_OrbVersion_HW_VERSION_PEARL_EV1;
            } else if (hardware_version_mv < 400) {
                // should be 0.30V = 300mV  (Mainboard 3.4)
                version = Hardware_OrbVersion_HW_VERSION_PEARL_EV4;
            } else if ((hardware_version_mv > 930) &&
                       (hardware_version_mv < 1130)) {
                // should be 0.64 V = 640 mV (Mainboard 3.6) but referenced
                // to 2.048 V because Mainboard 3.6 has a new 2.048 V voltage
                // reference connected to VREF+ instead of 3V3_UC.
                // -> Limits are adjusted to 3.3 V reference as configured in
                // device tree
                // -> 0.64 V * 3.3 V / 2.048 V =  1.03 V
                // -> lower limit = 1.03 V - 0.1 V = 0.93 V = 930 mV
                // -> upper limit = 1.03 V + 0.1 V = 1.13 V = 1130 mV
                version = Hardware_OrbVersion_HW_VERSION_PEARL_EV5;
            } else {
                LOG_ERR("Unknown main board from voltage: %umV",
                        hardware_version_mv);
            }
        }
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
        gpio_pin_configure_dt(&hw_main_board_bit0, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_main_board_bit1, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_main_board_bit2, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_main_board_bit3, GPIO_INPUT);

        int hw_bits = gpio_pin_get_dt(&hw_main_board_bit3) << 3 |
                      gpio_pin_get_dt(&hw_main_board_bit2) << 2 |
                      gpio_pin_get_dt(&hw_main_board_bit1) << 1 |
                      gpio_pin_get_dt(&hw_main_board_bit0);

        switch (hw_bits) {
        case 0:
            version = Hardware_OrbVersion_HW_VERSION_DIAMOND_POC1;
            break;
        case 1:
            version = Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2;
            break;
        case 2:
            version = Hardware_OrbVersion_HW_VERSION_DIAMOND_B3;
            break;
        default:
            LOG_ERR("Unknown main board from IO expander: %d", hw_bits);
            break;
        }

        // on diamond, fill in front unit and power board versions if
        // hw_version points to a valid memory location
        if (hw_version != NULL) {
            hw_version->front_unit = version_get_front_unit_rev();
            hw_version->power_board = version_get_power_board_rev();
        }
#endif
    }

    if (hw_version != NULL) {
        hw_version->version = version;
    }

    return RET_SUCCESS;
}

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
Hardware_FrontUnitVersion
version_get_front_unit_rev(void)
{
    static Hardware_FrontUnitVersion front_unit_version =
        Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN;

    if (front_unit_version ==
        Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN) {
        gpio_pin_configure_dt(&hw_front_unit_bit0, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_front_unit_bit1, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_front_unit_bit2, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_front_unit_bit3, GPIO_INPUT);

        int hw_bits = gpio_pin_get_dt(&hw_front_unit_bit3) << 3 |
                      gpio_pin_get_dt(&hw_front_unit_bit2) << 2 |
                      gpio_pin_get_dt(&hw_front_unit_bit1) << 1 |
                      gpio_pin_get_dt(&hw_front_unit_bit0);

        switch (hw_bits) {
        case 0:
            front_unit_version =
                Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_0;
            break;
        case 1:
            front_unit_version =
                Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_1;
            break;
        case 2:
            front_unit_version =
                Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2A;
            break;
        case 3:
            front_unit_version =
                Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B;
            break;
        default:
            LOG_ERR("Unknown front unit from IO expander: %d", hw_bits);
            front_unit_version =
                Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN;
        }
    }

    return front_unit_version;
}

Hardware_PowerBoardVersion
version_get_power_board_rev(void)
{
    static Hardware_PowerBoardVersion power_board_version =
        Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN;

    if (power_board_version ==
        Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN) {
        gpio_pin_configure_dt(&hw_pwr_board_bit0, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_pwr_board_bit1, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_pwr_board_bit2, GPIO_INPUT);
        gpio_pin_configure_dt(&hw_pwr_board_bit3, GPIO_INPUT);

        int hw_bits = gpio_pin_get_dt(&hw_pwr_board_bit3) << 3 |
                      gpio_pin_get_dt(&hw_pwr_board_bit2) << 2 |
                      gpio_pin_get_dt(&hw_pwr_board_bit1) << 1 |
                      gpio_pin_get_dt(&hw_pwr_board_bit0);

        switch (hw_bits) {
        case 0:
            power_board_version =
                Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_0;
            break;
        case 1:
            power_board_version =
                Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_1;
            break;
        case 2:
            power_board_version =
                Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_2;
            break;
        default:
            LOG_ERR("Unknown power board from IO expander: %d", hw_bits);
            power_board_version =
                Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN;
        }
    }

    return power_board_version;
}
#endif

Hardware_OrbVersion
version_get_hardware_rev(void)
{
    if (version == Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
        version_fetch_hardware_rev(NULL);
    }

    return version;
}

int
version_hw_send(uint32_t remote)
{
    Hardware hw = {.version = version};

    return publish_new(&hw, sizeof(hw), McuToJetson_hardware_tag, remote);
}

int
version_init(void)
{
    return version_fetch_hardware_rev(NULL);
}

#if CONFIG_MEMFAULT
#include <memfault/core/platform/device_info.h>
#include <stdio.h>

#ifdef CONFIG_BOARD_PEARL_MAIN
static const char hardware_versions_str[][14] = {
    "UNKNOWN", "PEARL_EV1", "PEARL_EV2", "PEARL_EV3", "PEARL_EV4", "PEARL_EV5",
};
static const char *software_type = "pearl-main-app";
#elif CONFIG_BOARD_DIAMOND_MAIN
static const char hardware_versions_str[][14] = {
    "UNKNOWN",
    "DIAMOND_POC1",
    "DIAMOND_POC2",
};
static const char *software_type = "diamond-main-app";
#endif

void
memfault_platform_get_device_info(sMemfaultDeviceInfo *info)
{
    static char version_str[32] = {0};
    struct image_version version = {0};
    dfu_version_primary_get(&version);

    snprintf(version_str, sizeof(version_str), "%u.%u.%u.%u", version.iv_major,
             version.iv_minor, version.iv_revision, version.iv_build_num);

    Hardware_OrbVersion hw_version = version_get_hardware_rev();
    size_t hardware_version_idx = (size_t)hw_version;
#if CONFIG_BOARD_DIAMOND_MAIN
    hardware_version_idx =
        hardware_version_idx - Hardware_OrbVersion_HW_VERSION_DIAMOND_POC1 + 1;
#endif

    // platform specific version information
    *info = (sMemfaultDeviceInfo){
        .device_serial = "0000",
        .software_type = software_type,
        .hardware_version = hardware_versions_str[hardware_version_idx],
        .software_version = version_str,
    };
}
#endif
