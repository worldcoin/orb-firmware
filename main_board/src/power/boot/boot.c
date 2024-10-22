#include "boot.h"
#include "optics/optics.h"
#include "orb_logs.h"
#include "sysflash/sysflash.h"
#include "system/version/version.h"
#include "temperature/fan/fan.h"
#include "ui/button/button.h"
#include "ui/rgb_leds/front_leds/front_leds.h"
#include "ui/rgb_leds/operator_leds/operator_leds.h"
#include "ui/rgb_leds/rgb_leds.h"
#include "utils.h"
#include <app_assert.h>
#include <app_config.h>
#include <bootutil/bootutil.h>
#include <dfu.h>
#include <errors.h>
#include <stdio.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_MEMFAULT
#include <memfault/core/reboot_tracking.h>
#endif

#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(power_sequence, CONFIG_POWER_SEQUENCE_LOG_LEVEL);

// Power supplies turned on in two phases:
// - Phase 1 initializes just enough power supplies to use the button and
//   operator LEDs
// - Phase 2 is for turning on all the power supplies. It is gated on the button
//   press unless we are booting after a reboot was commanded during an update.

K_THREAD_STACK_DEFINE(reboot_thread_stack, THREAD_STACK_SIZE_POWER_MANAGEMENT);
static struct k_thread reboot_thread_data;

#if defined(CONFIG_BOARD_PEARL_MAIN)
static const struct gpio_dt_spec supply_3v8_enable_rfid_irq_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v8_enable_rfid_irq_gpios);
static const struct gpio_dt_spec lte_gps_usb_reset_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(lte_gps_usb_reset), gpios);
#endif

static const struct gpio_dt_spec supply_3v3_ssd_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v3_ssd_enable_gpios);
static const struct gpio_dt_spec supply_3v3_wifi_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v3_wifi_enable_gpios);
static const struct gpio_dt_spec supply_12v_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_12v_enable_gpios);
static const struct gpio_dt_spec supply_5v_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_5v_enable_gpios);
static const struct gpio_dt_spec supply_3v3_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v3_enable_gpios);
static const struct gpio_dt_spec supply_1v8_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_1v8_enable_gpios);
static const struct gpio_dt_spec supply_pvcc_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_pvcc_enable_gpios);
static const struct gpio_dt_spec supply_super_cap_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_super_cap_enable_gpios);
static const struct gpio_dt_spec supply_vbat_sw_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_vbat_sw_enable_gpios);
static const struct gpio_dt_spec power_button_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(buttons, power_button), gpios);
static const struct gpio_dt_spec jetson_sleep_wake_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(jetson_power_pins, sleep_wake), gpios);
static const struct gpio_dt_spec jetson_power_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(jetson_power_pins, power_enable), gpios);
static const struct gpio_dt_spec jetson_system_reset_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(jetson_power_pins, system_reset), gpios);
static const struct gpio_dt_spec jetson_shutdown_request_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(jetson_power_pins, shutdown_request), gpios);
static const struct gpio_dt_spec supply_meas_enable_spec = GPIO_DT_SPEC_GET(
    DT_PATH(voltage_measurement), supply_voltages_meas_enable_gpios);
static const struct gpio_dt_spec pvcc_in_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pvcc_voltage_gpios);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
static const struct gpio_dt_spec supply_3v3_lte_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v3_lte_enable_gpios);
static const struct gpio_dt_spec supply_12v_caps_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_12v_caps_enable_gpios);
static const struct gpio_dt_spec supply_1v2_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_1v2_enable_gpios);
static const struct gpio_dt_spec supply_2v8_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_2v8_enable_gpios);
static const struct gpio_dt_spec supply_3v6_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v6_enable_gpios);
static const struct gpio_dt_spec supply_5v_rgb_enable_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_5v_rgb_enable_gpios);

static const struct gpio_dt_spec usb_hub_reset_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), usb_hub_reset_gpios);
#endif

static K_SEM_DEFINE(sem_reboot, 0, 1);
static atomic_t reboot_delay_s = ATOMIC_INIT(0);
static k_tid_t reboot_tid = NULL;
static struct gpio_callback shutdown_cb_data;

#define I2C_CLOCK_NODE  DT_PATH(zephyr_user)
#define I2C_CLOCK_CTLR  DT_GPIO_CTLR(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_PIN   DT_GPIO_PIN(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_FLAGS DT_GPIO_FLAGS(I2C_CLOCK_NODE, i2c_clock_gpios)

int
power_configure_gpios(void)
{
    orb_mcu_Hardware_OrbVersion version = version_get_hardware_rev();
    int ret;

    if (!device_is_ready(supply_vbat_sw_enable_gpio_spec.port) ||
        !device_is_ready(supply_12v_enable_gpio_spec.port) ||
        !device_is_ready(supply_5v_enable_gpio_spec.port) ||
#if defined(CONFIG_BOARD_PEARL_MAIN)
        !device_is_ready(supply_3v8_enable_rfid_irq_gpio_spec.port) ||
#endif
        !device_is_ready(supply_3v3_enable_gpio_spec.port) ||
        !device_is_ready(supply_1v8_enable_gpio_spec.port) ||
        !device_is_ready(supply_super_cap_enable_gpio_spec.port) ||
        !device_is_ready(supply_pvcc_enable_gpio_spec.port) ||
        !device_is_ready(power_button_gpio_spec.port) ||
        !device_is_ready(jetson_sleep_wake_gpio_spec.port) ||
        !device_is_ready(jetson_power_enable_gpio_spec.port) ||
        !device_is_ready(jetson_system_reset_gpio_spec.port) ||
        !device_is_ready(jetson_shutdown_request_gpio_spec.port) ||
        !device_is_ready(supply_meas_enable_spec.port)) {
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_vbat_sw_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_12v_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_5v_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

#if defined(CONFIG_BOARD_PEARL_MAIN)
    // 3.8V regulator only available on EV1...4
    if ((version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
        ret = gpio_pin_configure_dt(&supply_3v8_enable_rfid_irq_gpio_spec,
                                    GPIO_OUTPUT_INACTIVE);
        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }
    }

    ret = gpio_pin_configure_dt(&lte_gps_usb_reset_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }
#endif

    ret = gpio_pin_configure_dt(&supply_3v3_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_1v8_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_super_cap_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_pvcc_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&power_button_gpio_spec, GPIO_INPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&jetson_sleep_wake_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&jetson_power_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&jetson_system_reset_gpio_spec, GPIO_INPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&jetson_shutdown_request_gpio_spec, GPIO_INPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_meas_enable_spec, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5 and Diamond
    if (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5 ||
        version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2 ||
        version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_B3) {
        if (!device_is_ready(supply_3v3_ssd_enable_gpio_spec.port) ||
            !device_is_ready(supply_3v3_wifi_enable_gpio_spec.port)) {
            return RET_ERROR_INTERNAL;
        }

        ret = gpio_pin_configure_dt(&supply_3v3_ssd_enable_gpio_spec,
                                    GPIO_OUTPUT_INACTIVE);
        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }

        ret = gpio_pin_configure_dt(&supply_3v3_wifi_enable_gpio_spec,
                                    GPIO_OUTPUT_INACTIVE);
        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    if (!device_is_ready(supply_3v3_lte_enable_gpio_spec.port) ||
        !device_is_ready(supply_12v_caps_enable_gpio_spec.port) ||
        !device_is_ready(supply_1v2_enable_gpio_spec.port) ||
        !device_is_ready(supply_2v8_enable_gpio_spec.port) ||
        !device_is_ready(supply_3v6_enable_gpio_spec.port) ||
        !device_is_ready(supply_5v_rgb_enable_gpio_spec.port) ||
        !device_is_ready(usb_hub_reset_gpio_spec.port)) {
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_3v3_lte_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_12v_caps_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_1v2_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_2v8_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_3v6_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_5v_rgb_enable_gpio_spec,
                                GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&usb_hub_reset_gpio_spec, GPIO_OUTPUT_ACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }
#endif

    return RET_SUCCESS;
}

BUILD_ASSERT(SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY >
                 SYS_INIT_GPIO_CONFIG_PRIORITY,
             "GPIOs must be configured before using them.");

BUILD_ASSERT(SYS_INIT_POWER_SUPPLY_INIT_PRIORITY >
                 SYS_INIT_GPIO_CONFIG_PRIORITY,
             "GPIOs must be configured before using them.");

SYS_INIT(power_configure_gpios, POST_KERNEL, SYS_INIT_GPIO_CONFIG_PRIORITY);

void
power_vbat_5v_3v3_supplies_on(void)
{
    const struct device *i2c_clock = DEVICE_DT_GET(I2C_CLOCK_CTLR);

    // We configure this pin here before we enable 3.3v supply
    // just so that we can disable the automatically-enabled pull-up.
    // We must do this because providing a voltage to the 3.3v power supply
    // output before it is online can trigger the safety circuit.
    //
    // After this is configured, the I2C initialization will run and
    // re-configure this pin as SCL.
    if (gpio_pin_configure(i2c_clock, I2C_CLOCK_PIN,
                           GPIO_OUTPUT | I2C_CLOCK_FLAGS)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return;
    }

    gpio_pin_set_dt(&supply_vbat_sw_enable_gpio_spec, 1);
    LOG_INF("VBAT SW enabled");
    k_msleep(20);

    gpio_pin_set_dt(&supply_5v_enable_gpio_spec, 1);
    LOG_INF("5V power supply enabled");
    k_msleep(20);

    gpio_pin_set_dt(&supply_3v3_enable_gpio_spec, 1);
    LOG_INF("3.3V power supply enabled");
    k_msleep(20);
}

void
power_vbat_5v_3v3_supplies_off(void)
{
    gpio_pin_set_dt(&supply_vbat_sw_enable_gpio_spec, 0);
    LOG_INF("VBAT SW disabled");
    k_msleep(20);

    gpio_pin_set_dt(&supply_5v_enable_gpio_spec, 0);
    LOG_INF("5V power supply disabled");
    k_msleep(20);

    gpio_pin_set_dt(&supply_3v3_enable_gpio_spec, 0);
    LOG_INF("3.3V power supply disabled");
}

int
power_turn_on_power_supplies(void)
{
    orb_mcu_Hardware_OrbVersion version = version_get_hardware_rev();

    // might be a duplicate call, but it's preferable to be sure that
    // these supplies are on
    power_vbat_5v_3v3_supplies_on();

    // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5 and Diamond
    if (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5 ||
        version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2 ||
        version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_B3) {
        gpio_pin_set_dt(&supply_3v3_ssd_enable_gpio_spec, 1);
        LOG_INF("3.3V SSD power supply enabled");

        gpio_pin_set_dt(&supply_3v3_wifi_enable_gpio_spec, 1);
        LOG_INF("3.3V WIFI power supply enabled");
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    gpio_pin_set_dt(&supply_12v_caps_enable_gpio_spec, 1);
    LOG_INF("12V_CAPS enabled");

    gpio_pin_set_dt(&supply_5v_rgb_enable_gpio_spec, 1);
    LOG_INF("5V_RGB enabled");

    gpio_pin_set_dt(&supply_3v6_enable_gpio_spec, 1);
    LOG_INF("3V6 enabled");

    gpio_pin_set_dt(&supply_3v3_lte_enable_gpio_spec, 1);
    LOG_INF("3V3_LTE enabled");

    gpio_pin_set_dt(&supply_2v8_enable_gpio_spec, 1);
    LOG_INF("2V8 enabled");
#endif

    k_msleep(100);

    gpio_pin_set_dt(&supply_12v_enable_gpio_spec, 1);
    LOG_INF("12V enabled");

#if defined(CONFIG_BOARD_PEARL_MAIN)
    // 3.8V regulator only available on EV1...4
    if ((version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
        (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {

        gpio_pin_set_dt(&supply_3v8_enable_rfid_irq_gpio_spec, 1);
        LOG_INF("3.8V enabled");
    }
#endif

    gpio_pin_set_dt(&supply_1v8_enable_gpio_spec, 1);
    LOG_INF("1.8V power supply enabled");

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    gpio_pin_set_dt(&supply_1v2_enable_gpio_spec, 1);
    LOG_INF("1V2 enabled");
#endif

    k_msleep(100);

    return 0;
}

BUILD_ASSERT(CONFIG_I2C_INIT_PRIORITY > SYS_INIT_POWER_SUPPLY_INIT_PRIORITY,
             "I2C must be initialized _after_ the power supplies so that the "
             "safety circuit doesn't get tripped");

#ifdef CONFIG_GPIO_PCA95XX_INIT_PRIORITY
BUILD_ASSERT(
    CONFIG_GPIO_PCA95XX_INIT_PRIORITY < SYS_INIT_POWER_SUPPLY_INIT_PRIORITY,
    "GPIO expanders need to be initialized for enabling the power supplies");
BUILD_ASSERT(
    CONFIG_GPIO_PCA95XX_INIT_PRIORITY < SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY,
    "GPIO expanders need to be initialized for the button state to be polled.");

#ifdef CONFIG_I2C_INIT_PRIO_INST_1
BUILD_ASSERT(
    CONFIG_GPIO_PCA95XX_INIT_PRIORITY > CONFIG_I2C_INIT_PRIO_INST_1,
    "GPIO expanders need to be initialized after I2C3 because they are "
    "connected to the I2C bus.");

BUILD_ASSERT(DEVICE_DT_GET(DT_INST(1, st_stm32_i2c_v2)) ==
                 DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(gpio_exp_pwr_brd))),
             "GPIO expander to power board must be the one connected to I2C "
             "instance 1 (i2c3)\nif not, make sure to correctly define "
             "CONFIG_I2C_INIT_PRIO_INST_x");
#endif
#endif

SYS_INIT(power_turn_on_power_supplies, POST_KERNEL,
         SYS_INIT_POWER_SUPPLY_INIT_PRIORITY);

#define BUTTON_PRESS_TIME_MS 600

/**
 * @brief Wait for a button press before continuing boot.
 *
 * This function also performs eye circuitry self-test as soon as PVCC
 * is low enough. PVCC is high for a few seconds after Orb resets.
 * We don't want to block the usage of the button so the self-test might be
 * _skipped_ if the button is pressed while PVCC is still high as this would
 * end up in a very bad UX (PVCC can be high for up to 25 seconds after reset).
 * Logic level is considered low when GPIO pin goes below 1.88V, meaning
 * PVCC is actually below 17,68V before the voltage divider:
 *      1.88 * 442 / 47 = 17,68V
 * @return
 */
static int
power_until_button_press(void)
{
    int ret;
    bool self_test_pending = true;

    gpio_pin_set_dt(&supply_meas_enable_spec, 1);

    k_msleep(1);

    ret = gpio_pin_configure_dt(&pvcc_in_gpio_spec, GPIO_INPUT);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    const orb_mcu_main_RgbColor white = RGB_WHITE_BUTTON_PRESS;
    uint32_t operator_led_mask = 0;
    operator_leds_set_blocking(&white, operator_led_mask);
    LOG_INF("Waiting for button press of " TOSTR(BUTTON_PRESS_TIME_MS) "ms");
    for (size_t i = 0; i <= OPERATOR_LEDS_COUNT; ++i) {
        // check if pvcc is discharged to perform optics self test
        // the button must not be pressed to initiate the self test
        if (self_test_pending && operator_led_mask == 0 &&
            gpio_pin_get_dt(&pvcc_in_gpio_spec) == 0) {
            if (optics_self_test() == 0) {
                self_test_pending = false;
                gpio_pin_set_dt(&supply_meas_enable_spec, 0);
                k_msleep(1000);
            }
        }

        if (IS_ENABLED(CONFIG_INSTA_BOOT)) {
            power_vbat_5v_3v3_supplies_on();
            operator_led_mask = BIT_MASK(OPERATOR_LEDS_COUNT);
            operator_leds_set_blocking(&white, operator_led_mask);
            i = OPERATOR_LEDS_COUNT;
            break;
        }

        if (gpio_pin_get_dt(&power_button_gpio_spec) == 0) {
            if (i > 1) {
                LOG_INF("Press stopped.");
                power_vbat_5v_3v3_supplies_off();
                // give some time for the wifi module to reset correctly
                k_msleep(1000);
            }

            operator_led_mask = 0;
            i = 0;
        } else {
            operator_led_mask = (operator_led_mask << 1) | 1;
        }

        if (i == 1) {
            LOG_INF("Press started.");
            power_vbat_5v_3v3_supplies_on();
        }

        // update LEDs
        operator_leds_set_blocking(&white, operator_led_mask);

        k_msleep(BUTTON_PRESS_TIME_MS / OPERATOR_LEDS_COUNT);
    }

    // Disconnect PVCC pin from GPIO so that it can be used by the ADC in other
    // modules
    ret = gpio_pin_configure_dt(&pvcc_in_gpio_spec, GPIO_DISCONNECTED);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}

/**
 * Decide whether to wait for user to press the button to start the Orb
 * or to directly boot the Orb (after fresh update)
 * @param dev
 * @return error code
 */
int
app_init_state(void)
{
    int ret = 0;

    LOG_INF_IMM("Hello from " CONFIG_BOARD " :)");

    // read image status to know whether we are waiting for user to press
    // the button
    struct boot_swap_state primary_slot = {0};
    boot_read_swap_state_by_id(FLASH_AREA_IMAGE_PRIMARY(0), &primary_slot);

    LOG_DBG("Magic: %u, swap type: %u, image_ok: %u", primary_slot.magic,
            primary_slot.swap_type, primary_slot.image_ok);

    // give some time for the wifi module to reset correctly
    // without its power supply
    k_msleep(2000);

    // if FW image is confirmed, gate turning on power supplies on button press
    // otherwise, application have been updated and not confirmed, boot Jetson
    if (primary_slot.image_ok != BOOT_FLAG_UNSET ||
        primary_slot.magic == BOOT_MAGIC_UNSET) {
        ret = power_until_button_press();
    } else {
        LOG_INF_IMM("Firmware image not confirmed, confirming");

        power_vbat_5v_3v3_supplies_on();

        // FIXME image to be confirmed once MCU fully booted
        // Image is confirmed before we actually reboot the Orb
        // in case the MCU is rebooted due to a removed battery or insufficient
        // battery capacity. This is a temporary workaround until we
        // have fallback mechanism in place.
        dfu_primary_confirm();
    }
    LOG_INF_IMM("Booting system...");

    return ret;
}

SYS_INIT(app_init_state, POST_KERNEL, SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY);

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
BUILD_ASSERT(CONFIG_LED_STRIP_INIT_PRIORITY <
                 SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY,
             "initialize LED strip before waiting for button press as it needs "
             "the strip");
#endif

#define SYSTEM_RESET_UI_DELAY_MS 200

/// SHUTDOWN_REQ interrupt callback
/// From the Jetson Datasheet DS-10184-001 § 2.6.2 Power Down
///     > When the baseboard sees low SHUTDOWN_REQ*, it should deassert POWER_EN
///     as soon as possible.
static void
shutdown_requested(const struct device *dev, struct gpio_callback *cb,
                   uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);

    if (pins & BIT(jetson_shutdown_request_gpio_spec.pin)) {
        gpio_pin_set_dt(&jetson_power_enable_gpio_spec, 0);

        // offload reboot to power management thread
        atomic_set(&reboot_delay_s, 1);
        // wake up reboot thread in case already waiting for the reboot
        // this will make the current event take precedence over the
        // currently pending reboot as the reboot thread will now
        // sleep for `reboot_delay_s` second before rebooting
        k_wakeup(reboot_tid);
        k_sem_give(&sem_reboot);

        LOG_INF("Jetson shut down");

#ifdef CONFIG_MEMFAULT
        MEMFAULT_REBOOT_MARK_RESET_IMMINENT(kMfltRebootReason_UserShutdown);
#endif
    }
}

static void
reboot_thread()
{
    uint32_t delay = 0;

    orb_mcu_Hardware_OrbVersion version = version_get_hardware_rev();

    // wait until triggered
    k_sem_take(&sem_reboot, K_FOREVER);

    struct boot_swap_state secondary_slot = {0};
    boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SECONDARY(0), &secondary_slot);
    LOG_DBG("Secondary Magic: %u, swap type: %u, image_ok: %u",
            secondary_slot.magic, secondary_slot.swap_type,
            secondary_slot.image_ok);

    // wait a second to display "shutdown" mode UI
    // to make sure Core is done sending UI commands
    delay = atomic_get(&reboot_delay_s);
    if (delay > 1) {
        k_msleep(1000);
        atomic_set(&reboot_delay_s, delay - 1);
        orb_mcu_main_RgbColor color = RGB_WHITE_SHUTDOWN;
        operator_leds_set_pattern(
            orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_PULSING_RGB,
            0b00100, &color);
    }

    do {
        // check if shutdown_pin is active, if so, it means Jetson
        // needs proper shutdown
        if (gpio_pin_get_dt(&jetson_shutdown_request_gpio_spec) == 1) {
            // From the Jetson Datasheet DS-10184-001 § 2.6.2 Power Down:
            //  > Once POWER_EN is deasserted, the module will assert
            //  SYS_RESET*, and the baseboard may shut down. SoC 3.3V I/O must
            //  reach 0.5V or lower at most 1.5ms after SYS_RESET* is asserted.
            //  SoC 1.8V I/O must reach 0.5V or lower at most 4ms after
            //  SYS_RESET* is asserted.
            while (gpio_pin_get_dt(&jetson_system_reset_gpio_spec) == 0)
                ;

            gpio_pin_set_dt(&supply_3v3_enable_gpio_spec, 0);
            // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5 and
            // Diamond
            if (version == orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5 ||
                version ==
                    orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_POC2 ||
                version == orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_B3) {
                gpio_pin_set_dt(&supply_3v3_ssd_enable_gpio_spec, 0);
                gpio_pin_set_dt(&supply_3v3_wifi_enable_gpio_spec, 0);
            }
            gpio_pin_set_dt(&supply_1v8_enable_gpio_spec, 0);

            // the jetson has been turned off following the specs, we can now
            // wait `reboot_delay_s` to reset
        }

        delay = atomic_get(&reboot_delay_s);
        LOG_INF("Rebooting in %u seconds", delay);
    } while (k_msleep(delay * 1000 - SYSTEM_RESET_UI_DELAY_MS) != 0);

    fan_turn_off();
    operator_leds_set_pattern(
        orb_mcu_main_DistributorLEDsPattern_DistributorRgbLedPattern_OFF, 0,
        NULL);
    front_leds_turn_off_blocking();

    k_msleep(SYSTEM_RESET_UI_DELAY_MS);

    LOG_INF("Going down!");

#if defined(CONFIG_LOG) && !defined(CONFIG_LOG_MODE_MINIMAL)
    uint32_t log_buffered_count = log_buffered_cnt();
    while (LOG_PROCESS() && --log_buffered_count)
        ;
#endif

    NVIC_SystemReset();
}

static int
shutdown_req_init(void)
{
    // Jetson is launched, we can now activate shutdown detection
    int ret;

    ret = gpio_pin_interrupt_configure_dt(&jetson_shutdown_request_gpio_spec,
                                          GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&shutdown_cb_data, shutdown_requested,
                       BIT(jetson_shutdown_request_gpio_spec.pin));
    ret = gpio_add_callback_dt(&jetson_shutdown_request_gpio_spec,
                               &shutdown_cb_data);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

__unused static int
shutdown_req_uninit(void)
{
    int ret = gpio_pin_interrupt_configure_dt(
        &jetson_shutdown_request_gpio_spec, GPIO_INT_DISABLE);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    ret = gpio_remove_callback_dt(&jetson_shutdown_request_gpio_spec,
                                  &shutdown_cb_data);
    if (ret) {
        ASSERT_SOFT(ret);
    }
    return ret;
}

int
boot_turn_on_jetson(void)
{
    LOG_INF("Enabling Jetson power");
    gpio_pin_set_dt(&jetson_power_enable_gpio_spec, 1);

    LOG_INF("Waiting for reset done signal from Jetson");
    while (gpio_pin_get_dt(&jetson_system_reset_gpio_spec) != 0)
        ;
    LOG_INF("Reset done");

    LOG_INF("Setting Jetson to WAKE mode");
    gpio_pin_set_dt(&jetson_sleep_wake_gpio_spec, 1);

#if defined(CONFIG_BOARD_PEARL_MAIN)
    LOG_INF("Enabling LTE, GPS, and USB");
    gpio_pin_set_dt(&lte_gps_usb_reset_gpio_spec, 0);
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
    LOG_INF("Enabling USB");
    gpio_pin_set_dt(&usb_hub_reset_gpio_spec, 0);
#endif

    shutdown_req_init();

    // Spawn reboot thread
    reboot_tid = k_thread_create(
        &reboot_thread_data, reboot_thread_stack,
        K_THREAD_STACK_SIZEOF(reboot_thread_stack), reboot_thread, NULL, NULL,
        NULL, THREAD_PRIORITY_POWER_MANAGEMENT, 0, K_NO_WAIT);
    k_thread_name_set(reboot_tid, "reboot");

    return RET_SUCCESS;
}

int
boot_turn_on_super_cap_charger(void)
{
    int ret = gpio_pin_set_dt(&supply_super_cap_enable_gpio_spec, 1);
    if (ret) {
        return ret;
    }
    LOG_INF("super cap charger enabled");

    k_msleep(1000);
    return RET_SUCCESS;
}

int
boot_turn_off_pvcc(void)
{
    gpio_pin_set_dt(&supply_pvcc_enable_gpio_spec, 0);
    LOG_INF("PVCC disabled");

    return RET_SUCCESS;
}

int
boot_turn_on_pvcc(void)
{
    gpio_pin_set_dt(&supply_pvcc_enable_gpio_spec, 1);
    LOG_INF("PVCC enabled");
    return RET_SUCCESS;
}

int
reboot(uint32_t delay_s)
{
    if (reboot_tid == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    } else {
        atomic_set(&reboot_delay_s, delay_s);
        // wake up reboot thread in case already waiting for the reboot
        // this will make the current event take precedence over the
        // currently pending reboot as the reboot thread will now
        // sleep for `reboot_delay_s` seconds before rebooting
        k_wakeup(reboot_tid);
        k_sem_give(&sem_reboot);
    }

    return RET_SUCCESS;
}
