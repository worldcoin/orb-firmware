#include "optics.h"
#include "logs_can.h"
#include "optics/1d_tof/tof_1d.h"
#include "optics/ir_camera_system/ir_camera_system.h"
#include "optics/liquid_lens/liquid_lens.h"
#include "optics/mirror/mirror.h"
#include "power/boot/boot.h"
#include "pubsub/pubsub.h"
#include "system/diag.h"
#include <app_assert.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(optics);

static HardwareDiagnostic_Status self_test_status =
    HardwareDiagnostic_Status_STATUS_UNKNOWN;

static ATOMIC_DEFINE(fu_pvcc_enabled, 1);
#define ATOMIC_FU_PVCC_ENABLED_BIT 0

// pin allows us to check whether PVCC is enabled on the front unit
// PVCC might be disabled by hardware due to an intense usage of the IR LEDs
// that doesn't respect the eye safety constraints
static struct gpio_dt_spec front_unit_pvcc_enabled = GPIO_DT_SPEC_GET_BY_IDX(
    DT_PATH(zephyr_user), front_unit_pvcc_enabled_gpios, 0);

#if defined(CONFIG_BOARD_PEARL_MAIN)

static struct gpio_callback fu_pvcc_enabled_cb_data;

static void
front_unit_pvcc_update(struct k_work *work);

static K_WORK_DEFINE(front_unit_pvcc_event_work, front_unit_pvcc_update);

/// Handle PVCC/3v3 line modifications on the front unit
/// Communicate change to Jetson
static void
front_unit_pvcc_update(struct k_work *work)
{
    ARG_UNUSED(work);

    HardwareDiagnostic_Status status = HardwareDiagnostic_Status_STATUS_OK;

    bool pvcc_available = (gpio_pin_get_dt(&front_unit_pvcc_enabled) != 0);
    if (pvcc_available) {
        atomic_set_bit(fu_pvcc_enabled, ATOMIC_FU_PVCC_ENABLED_BIT);
        LOG_INF("Circuitry allows usage of IR LEDs");
    } else {
        atomic_clear_bit(fu_pvcc_enabled, ATOMIC_FU_PVCC_ENABLED_BIT);

        status = HardwareDiagnostic_Status_STATUS_OK;
        LOG_WRN("Eye safety circuitry tripped");
    }

    diag_set_status(HardwareDiagnostic_Source_OPTICS_IR_LEDS, status);
}

static void
interrupt_fu_pvcc_handler(const struct device *dev, struct gpio_callback *cb,
                          uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(front_unit_pvcc_enabled.pin)) {
        k_work_submit(&front_unit_pvcc_event_work);
    }
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
#endif

bool
optics_usable(void)
{
    atomic_val_t pvcc_enabled = atomic_get(fu_pvcc_enabled);
    return ((pvcc_enabled & BIT(ATOMIC_FU_PVCC_ENABLED_BIT)) != 0) &&
           distance_is_safe();
}

bool
optics_safety_circuit_triggered(void)
{
    atomic_val_t pvcc_enabled = atomic_get(fu_pvcc_enabled);
    return !pvcc_enabled;
}

int
optics_init(const Hardware *hw_version)
{
    int err_code;

    diag_set_status(
        HardwareDiagnostic_Source_OPTICS_EYE_SAFETY_CIRCUIT_SELF_TEST,
        self_test_status);

    err_code = ir_camera_system_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = mirror_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = liquid_lens_init(hw_version);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

    err_code = tof_1d_init();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }

#if defined(CONFIG_BOARD_PEARL_MAIN)

    // TODO poll pvcc-enable pin on Diamond
    err_code = configure_front_unit_3v3_detection();
    if (err_code) {
        ASSERT_SOFT(err_code);
        return err_code;
    }
#else
    int ret = gpio_pin_configure_dt(&front_unit_pvcc_enabled, GPIO_INPUT);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    if (gpio_pin_get_dt(&front_unit_pvcc_enabled) != 0) {
        LOG_INF("Circuitry allows usage of IR LEDs");
    } else {
        LOG_WRN("Eye safety circuitry tripped");
    }

    UNUSED_PARAMETER(hw_version);
#endif

    return err_code;
}

int
optics_self_test(void)
{
#if defined(CONFIG_BOARD_PEARL_MAIN)
    static const struct gpio_dt_spec ir_leds_gpios[] = {
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 0),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 1),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 2),
        GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user),
                                tests_ir_leds_850_940_gpios, 3),
    };

    static const char *ir_leds_names[] = {
        "ir_850nm_left",
        "ir_850nm_right",
        "ir_940nm_left",
        "ir_940nm_right",
    };

    int ret;

    if (self_test_status == HardwareDiagnostic_Status_STATUS_OK ||
        self_test_status == HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE) {
        return RET_ERROR_ALREADY_INITIALIZED;
    }

    // turn on IR LED subsets one by one, by driving GPIO pins, to check
    // that all lines are making the eye safety circuitry trip
    ret = gpio_pin_configure_dt(
        &front_unit_pvcc_enabled,
        GPIO_INPUT); // todo: on Diamond we need to enable +3V3 before reading
                     // the Front Unit IO expander
    if (ret) {
        self_test_status =
            HardwareDiagnostic_Status_STATUS_INITIALIZATION_ERROR;
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    self_test_status = HardwareDiagnostic_Status_STATUS_OK;
    for (size_t i = 0; i < ARRAY_SIZE(ir_leds_gpios) &&
                       self_test_status == HardwareDiagnostic_Status_STATUS_OK;
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
            self_test_status = HardwareDiagnostic_Status_STATUS_SAFETY_ISSUE;
        } else {
            LOG_INF("%s tripped safety circuitry", ir_leds_names[i]);
        }

        // eye safety circuitry to reset
        power_vbat_5v_3v3_supplies_off();
    }

    return RET_SUCCESS;
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    // todo: implement for Diamond hardware
    return RET_SUCCESS;
#endif
}
