#include "version.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <dfu.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(version, CONFIG_VERSION_LOG_LEVEL);

// Hardware version can be fetched using UC_ADC_HW_VERSION on the main board:
// - 3.0 firmware is specific, so we can provide an hardcoded implementation
// - v3.1 pull down
// - v3.2 pull up
// GPIO logic level can then be used to get the hardware version

const struct adc_dt_spec adc_dt_spec = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

#define ADC_RESOLUTION       12
#define ADC_GAIN             ADC_GAIN_1
#define ADC_REFERENCE        ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

ret_code_t
version_get_hardware_rev(enum hw_version_e *hw_version)
{
    static uint16_t version = 0;

    // read only once and keep hardware version into `version`
    if (version == 0) {
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

        int32_t sample_buffer;
        struct adc_sequence sequence = {
            .buffer = &sample_buffer,
            .buffer_size = sizeof(sample_buffer),
            .channels = BIT(adc_dt_spec.channel_id),
            .resolution = ADC_RESOLUTION,
            .oversampling = 0,
        };

        int err_code = adc_read(adc_dt_spec.dev, &sequence);
        if (err_code < 0) {
            return RET_ERROR_INTERNAL;
        } else {
            adc_raw_to_millivolts(vref_mv, ADC_GAIN_1, ADC_RESOLUTION,
                                  &sample_buffer);

            int32_t hardware_version_mv = sample_buffer;
            LOG_DBG("Hardware rev voltage: %dmV", sample_buffer);

            if (hardware_version_mv > 3200) {
                // should be 3.3V
                *hw_version = HW_VERSION_MAINBOARD_EV2;
            } else if (hardware_version_mv > 2900) {
                // should be 3.0V
                *hw_version = HW_VERSION_MAINBOARD_EV3;
            } else if (hardware_version_mv < 100) {
                // should be 0.0V
                *hw_version = HW_VERSION_MAINBOARD_EV1;
            } else {
                LOG_ERR("Unknown main board from voltage: %umV",
                        hardware_version_mv);
            }
        }
    }

    return RET_SUCCESS;
}

int
version_send(uint32_t remote)
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
