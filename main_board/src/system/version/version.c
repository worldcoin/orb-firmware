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
 * - v4.3 p[13..10] = 3
 * - v4.4 p[13..10] = 4 // evt
 * - v4.5 p[13..10] = 5 // dvt
 *
 * ## Front unit
 * Hardware version can be fetched using IO expander on the front unit:
 * - v6.0 p[13..10] = 0
 * - v6.1 p[13..10] = 1
 * - v6.2A p[13..10] = 2
 * - v6.2B p[13..10] = 3
 * - v6.3A p[13..10] = 4 // evt
 * - v6.3B p[13..10] = 5 // evt
 * - v6.3C p[13..10] = 7 // evt
 * - v6.3D p[13..10] = 8 // dvt
 *
 * ## Power board
 * Hardware version can be fetched using IO expander on the power board:
 * - v1.0: p[13..10] = 0
 * - v1.1: p[13..10] = 1
 * - v1.2: p[13..10] = 2
 * - v1.3: p[13..10] = 3
 * - v1.4: p[13..10] = 4 // evt
 * - v1.5: p[13..10] = 5 // dvt
 **/

#if defined(CONFIG_BOARD_PEARL_MAIN)
const struct adc_dt_spec adc_dt_spec = ADC_DT_SPEC_GET(DT_PATH(board_version));

#define ADC_RESOLUTION_BITS  12
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
static const struct gpio_dt_spec hw_main_board_bit[4] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_main_board_gpios,
                            0),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_main_board_gpios,
                            1),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_main_board_gpios,
                            2),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_main_board_gpios,
                            3),
};

static const struct gpio_dt_spec hw_front_unit_bit[4] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_front_unit_gpios,
                            0),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_front_unit_gpios,
                            1),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_front_unit_gpios,
                            2),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_front_unit_gpios,
                            3),
};

static const struct gpio_dt_spec hw_pwr_board_bit[4] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_pwr_board_gpios,
                            0),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_pwr_board_gpios,
                            1),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_pwr_board_gpios,
                            2),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_pwr_board_gpios,
                            3),
};

static const struct gpio_dt_spec reset_board_bit[1] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), hw_version_reset_board_gpios,
                            0),
};
#endif

static orb_mcu_Hardware board_versions = {
    .version = orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN,
    .front_unit = orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN,
    .power_board =
        orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN,
    .reset_board =
        orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN};

int
version_fw_send(uint32_t remote)
{
    struct image_version version = {0};

    dfu_version_primary_get(&version);

    orb_mcu_Versions versions = {
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

    return publish_new(&versions, sizeof(versions),
                       orb_mcu_main_McuToJetson_versions_tag, remote);
}

__maybe_unused static int
get_hw_bits(const struct gpio_dt_spec *pins_bit, const size_t len, int *hw_bits)
{
    if (len > 4) {
        return RET_ERROR_FORBIDDEN;
    }

    *hw_bits = 0;

    for (size_t i = 0; i < len; i++) {
        if (!device_is_ready(pins_bit[i].port)) {
            return RET_ERROR_INVALID_STATE;
        }

        const int ret = gpio_pin_configure_dt(&pins_bit[i], GPIO_INPUT);
        if (ret != 0) {
            LOG_ERR("Failed to configure pin %d: %u", i, pins_bit[i].pin);
            return RET_ERROR_INTERNAL;
        }

        const int pin = gpio_pin_get_dt(&pins_bit[i]);
        if (pin < 0) {
            LOG_ERR("Failed to get pin %d: %d", i, pin);
            return RET_ERROR_INTERNAL;
        }
        *hw_bits |= pin << i;
    }

    return RET_SUCCESS;
}

static ret_code_t
version_fetch_hardware_rev(orb_mcu_Hardware *hw_version)
{
    if (hw_version == NULL) {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
        return RET_ERROR_INVALID_PARAM;
    }

    // read only once and keep hardware version into `version`
    if (hw_version->version == orb_mcu_Hardware_OrbVersion_HW_VERSION_UNKNOWN) {
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
                hw_version->version =
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV2;
            } else if (hardware_version_mv > 2900) {
                // should be 3.0V = 3000mV (Mainboard 3.3)
                hw_version->version =
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV3;
            } else if (hardware_version_mv < 100) {
                // should be 0.0V (Mainboard 3.1)
                hw_version->version =
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV1;
            } else if (hardware_version_mv < 400) {
                // should be 0.30V = 300mV  (Mainboard 3.4)
                hw_version->version =
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV4;
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
                hw_version->version =
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5;
            } else {
                LOG_ERR("Unknown main board from voltage: %umV",
                        hardware_version_mv);
            }
        }
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
        int hw_bits;
        const int ret = get_hw_bits(hw_main_board_bit,
                                    ARRAY_SIZE(hw_main_board_bit), &hw_bits);
        if (ret != RET_SUCCESS) {
            return ret;
        }

        switch (hw_bits) {
        case 0:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC1;
            break;
        case 1:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2;
            break;
        case 2:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_B3;
            break;
        case 3:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_EVT;
            break;
        case 4:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_4;
            break;
        case 5:
            hw_version->version =
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_5;
            break;
        default:
            LOG_ERR("Unknown main board from IO expander: %d", hw_bits);
            break;
        }
#endif
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (hw_version->front_unit ==
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN) {
        hw_version->front_unit = version_get_front_unit_rev();
    }

    if (hw_version->power_board ==
        orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN) {
        hw_version->power_board = version_get_power_board_rev();
    }

    if (hw_version->reset_board ==
        orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN) {
        hw_version->reset_board = version_get_reset_board_rev();
    }
#endif

    return RET_SUCCESS;
}

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
orb_mcu_Hardware_FrontUnitVersion
version_get_front_unit_rev(void)
{
    static orb_mcu_Hardware_FrontUnitVersion front_unit_version =
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN;

    if (front_unit_version ==
        orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN) {
        int hw_bits;
        const int ret = get_hw_bits(hw_front_unit_bit,
                                    ARRAY_SIZE(hw_front_unit_bit), &hw_bits);
        if (ret != RET_SUCCESS) {
            return orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN;
        }

        switch (hw_bits) {
        case 0:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_0;
            break;
        case 1:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_1;
            break;
        case 2:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2A;
            break;
        case 3:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_2B;
            break;
        case 4:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3A;
            break;
        case 5:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3B;
            break;
        case 7:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3C;
            break;
        case 8:
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3D;
            break;
        default:
            LOG_ERR("Unknown front unit from IO expander: %d", hw_bits);
            front_unit_version =
                orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_UNKNOWN;
        }
    }

    return front_unit_version;
}

orb_mcu_Hardware_PowerBoardVersion
version_get_power_board_rev(void)
{
    static orb_mcu_Hardware_PowerBoardVersion power_board_version =
        orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN;

    if (power_board_version ==
        orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN) {
        int hw_bits;
        const int ret = get_hw_bits(hw_pwr_board_bit,
                                    ARRAY_SIZE(hw_pwr_board_bit), &hw_bits);
        if (ret != RET_SUCCESS) {
            ASSERT_SOFT(ret);
            return orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN;
        }

        switch (hw_bits) {
        case 0:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_0;
            break;
        case 1:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_1;
            break;
        case 2:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_2;
            break;
        case 3:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_3;
            break;
        case 4:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_4;
            break;
        case 5:
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_V1_5;
            break;
        default:
            LOG_ERR("Unknown power board from IO expander: %d", hw_bits);
            power_board_version =
                orb_mcu_Hardware_PowerBoardVersion_POWER_BOARD_VERSION_UNKNOWN;
        }
    }

    return power_board_version;
}

orb_mcu_Hardware_ResetBoardVersion
version_get_reset_board_rev(void)
{
    static orb_mcu_Hardware_ResetBoardVersion reset_board_version =
        orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN;

    if (reset_board_version ==
        orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN) {
        int hw_bits;
        const int ret =
            get_hw_bits(reset_board_bit, ARRAY_SIZE(reset_board_bit), &hw_bits);
        if (ret != RET_SUCCESS) {
            ASSERT_SOFT(ret);
            return orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN;
        }

        switch (hw_bits) {
        case 0:
            reset_board_version =
                orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_V1_1;
            break;
        default:
            LOG_ERR("Unknown reset board from IO expander: %d", hw_bits);
            reset_board_version =
                orb_mcu_Hardware_ResetBoardVersion_RESET_BOARD_VERSION_UNKNOWN;
        }
    }

    return reset_board_version;
}

#endif

orb_mcu_Hardware
version_get(void)
{
    // see if any of the version needs to be fetched again
    version_fetch_hardware_rev(&board_versions);

    return board_versions;
}

int
version_hw_send(uint32_t remote)
{
    return publish_new(&board_versions, sizeof(board_versions),
                       orb_mcu_main_McuToJetson_hardware_tag, remote);
}

int
version_init(void)
{
    return version_fetch_hardware_rev(&board_versions);
}

#if CONFIG_MEMFAULT
#include <memfault/core/platform/device_info.h>
#include <stdio.h>

#ifdef CONFIG_BOARD_PEARL_MAIN
static const char hardware_versions_str[][14] = {
    "PEARL_UNKNOWN", "PEARL_EV1", "PEARL_EV2", "PEARL_EV3",
    "PEARL_EV4",     "PEARL_EV5", "PEARL_EV6",
};
static const char *software_type = "pearl-main-app";
#elif CONFIG_BOARD_DIAMOND_MAIN
static const char hardware_versions_str[][16] = {
    "DIAMOND_UNKNOWN", "DIAMOND_POC1",    "DIAMOND_POC2",
    "DIAMOND_B3",      "DIAMOND_EVT_4.3", "DIAMOND_EVT_4.4"};
static const char *software_type = "diamond-main-app";

BUILD_ASSERT(ARRAY_SIZE(hardware_versions_str) >=
             orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_5 -
                 orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC1 + 1);

#endif

void
memfault_platform_get_device_info(sMemfaultDeviceInfo *info)
{
    // report error only once
    static bool hardware_version_error = false;

    const char *version_str = STRINGIFY(FW_VERSION_FULL);
    orb_mcu_Hardware hw_version = version_get();
    size_t hardware_version_idx = (size_t)hw_version.version;
#if CONFIG_BOARD_DIAMOND_MAIN
    hardware_version_idx = hardware_version_idx -
                           orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC1 +
                           1;
#endif

    if (hardware_version_idx > ARRAY_SIZE(hardware_versions_str)) {
        if (hardware_version_error == false) {
            ASSERT_SOFT(RET_ERROR_NOT_FOUND);
            hardware_version_error = true;
        }
        hardware_version_idx = 0;
    }

    // platform specific version information
    *info = (sMemfaultDeviceInfo){
        .device_serial = "0000",
        .software_type = software_type,
        .hardware_version = hardware_versions_str[hardware_version_idx],
        .software_version = version_str,
    };
}
#endif
