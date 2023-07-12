#include "optics.h"
#include "app_config.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirrors/mirrors.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(optics);

static OpticsDiagnostic_Status self_test_status =
    OpticsDiagnostic_Status_OPTICS_INITIALIZATION_ERROR;

// pin allows us to check whether PVCC is enabled on the front unit
// PVCC might be disabled by hardware due to an intense usage of the IR LEDs
// that doesn't respect the eye safety constraints
static struct gpio_dt_spec front_unit_pvcc_enabled = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), front_unit_pvcc_enabled_gpios, 0);

static struct gpio_callback fu_pvcc_enabled_cb_data;

static ATOMIC_DEFINE(fu_pvcc_enabled, 1);
#define ATOMIC_FU_PVCC_ENABLED_BIT 0

static void
front_unit_pvcc_update(struct k_work *work);

static K_WORK_DEFINE(front_unit_pvcc_event_work, front_unit_pvcc_update);

/// Handle PVCC/3v3 line modifications on the front unit
/// Communicate change to Jetson
static void
front_unit_pvcc_update(struct k_work *work)
{
    ARG_UNUSED(work);

    OpticsDiagnostic optics_diag = {
        .source = OpticsDiagnostic_Source_OPTICS_IR_LEDS,
        .status = OpticsDiagnostic_Status_OPTICS_OK,
    };

    bool pvcc_available = (gpio_pin_get_dt(&front_unit_pvcc_enabled) != 0);
    if (pvcc_available) {
        atomic_set_bit(fu_pvcc_enabled, ATOMIC_FU_PVCC_ENABLED_BIT);
        LOG_INF("Circuitry allows usage of IR LEDs");
    } else {
        atomic_clear_bit(fu_pvcc_enabled, ATOMIC_FU_PVCC_ENABLED_BIT);

        optics_diag.status = OpticsDiagnostic_Status_OPTICS_OK;
        LOG_WRN("Eye safety circuitry tripped");
    }

    publish_new(&optics_diag, sizeof(optics_diag), McuToJetson_optics_diag_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
interrupt_fu_pvcc_handler(const struct device *dev, struct gpio_callback *cb,
                          uint32_t pins)
{
    if (pins & BIT(front_unit_pvcc_enabled.pin)) {
        k_work_submit(&front_unit_pvcc_event_work);
    }
}

bool
optics_usable(void)
{
    atomic_val_t pvcc_enabled = atomic_get(fu_pvcc_enabled);
    return ((pvcc_enabled & BIT(ATOMIC_FU_PVCC_ENABLED_BIT)) != 0) &&
           distance_is_safe();
}

static int
configure_front_unit_3v3_detection(void)
{
    int ret = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&front_unit_pvcc_enabled,
                                          GPIO_INT_EDGE_BOTH);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return ret;
    }

    gpio_init_callback(&fu_pvcc_enabled_cb_data, interrupt_fu_pvcc_handler,
                       BIT(front_unit_pvcc_enabled.pin));
    ret = gpio_add_callback(front_unit_pvcc_enabled.port,
                            &fu_pvcc_enabled_cb_data);
    if (ret != 0) {
        ASSERT_SOFT(ret);
    }

    // set initial state
    k_work_submit(&front_unit_pvcc_event_work);

    return ret;
}

int
optics_init(void)
{
    int err_code;

    // report self test status to Jetson
    OpticsDiagnostic optics_diag = {
        .source = OpticsDiagnostic_Source_OPTICS_EYE_SAFETY_CIRCUIT_SELF_TEST,
        .status = self_test_status,
    };
    publish_new(&optics_diag, sizeof(optics_diag), McuToJetson_optics_diag_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

    err_code = ir_camera_system_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = mirrors_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = liquid_lens_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = tof_1d_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = configure_front_unit_3v3_detection();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    return err_code;
}

static struct gpio_dt_spec ir_leds_gpios[] = {
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), tests_ir_leds_850_940_gpios,
                            0),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), tests_ir_leds_850_940_gpios,
                            1),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), tests_ir_leds_850_940_gpios,
                            2),
    GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), tests_ir_leds_850_940_gpios,
                            3),
};

static const char *ir_leds_names[] = {
    "ir_850nm_left",
    "ir_850nm_right",
    "ir_940nm_left",
    "ir_940nm_right",
};

static int
optics_self_test(void)
{
    // turn on IR LED subsets one by one, by driving GPIO pins, to check
    // that all lines are making the eye safety circuitry trip
    self_test_status = OpticsDiagnostic_Status_OPTICS_OK;
    for (size_t i = 0; i < ARRAY_SIZE(ir_leds_gpios) &&
                       self_test_status == OpticsDiagnostic_Status_OPTICS_OK;
         i++) {
        power_vbat_5v_3v3_supplies_on();
        boot_turn_off_pvcc();

        gpio_pin_configure_dt(&ir_leds_gpios[i], GPIO_OUTPUT);
        gpio_pin_set_dt(&ir_leds_gpios[i], 1);
        k_msleep(250);
        gpio_pin_set_dt(&ir_leds_gpios[i], 0);
        k_msleep(250);

        bool pvcc_available = (gpio_pin_get_dt(&front_unit_pvcc_enabled) != 0);
        if (pvcc_available) {
            // eye safety circuitry doesn't respond to self test
            LOG_ERR("%s didn't disable PVCC via eye safety circuitry",
                    ir_leds_names[i]);
            self_test_status = OpticsDiagnostic_Status_OPTICS_SAFETY_ISSUE;
        } else {
            LOG_INF("%s tripped safety circuitry", ir_leds_names[i]);
        }

        // eye safety circuitry to reset
        power_vbat_5v_3v3_supplies_off();
    }

    return 0;
}

SYS_INIT(optics_self_test, POST_KERNEL,
         SYS_INIT_EYE_SAFETY_CIRCUITRY_SELFTEST_PRIORITY);
