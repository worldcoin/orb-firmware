#include "version.h"
#include "mcu_messaging.pb.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <can_messaging.h>
#include <dfu.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(version);

// Hardware version can be fetched using UC_ADC_HW_VERSION on the main board:
// - 3.0 firmware is specific, so we can provide an hardcoded implementation
// - v3.1 pull down
// - v3.2 pull up
// GPIO logic level can then be used to get the hardware version

ret_code_t
version_get_hardware_rev(uint16_t *hw_version)
{
#if defined(CONFIG_BOARD_MCU_MAIN_V30)
    *hw_version = 30;
    return RET_SUCCESS;
#elif defined(CONFIG_BOARD_MCU_MAIN_V31)
    static uint16_t version = 0;

    // read only once and keep hardware version into `version`
    if (version == 0) {
        const struct gpio_dt_spec hardware_version_gpio =
            GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), hw_version_gpios);
        if (!device_is_ready(hardware_version_gpio.port)) {
            return RET_ERROR_NOT_INITIALIZED;
        }

        int err_code = gpio_pin_configure_dt(&hardware_version_gpio, 0);
        if (err_code != 0) {
            ASSERT_SOFT(err_code);
            return RET_ERROR_INTERNAL;
        }

        bool is_3_2 = gpio_pin_get_dt(&hardware_version_gpio);
        if (is_3_2) {
            version = 32;
        } else {
            version = 31;
        }
    }

    *hw_version = version;

    return RET_SUCCESS;
#else
#error board unknown
#endif
}

int
version_send(void)
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
                       CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}
