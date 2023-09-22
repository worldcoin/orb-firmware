#include "boot.h"
#include "logs_can.h"
#include "optics/optics.h"
#include "sysflash/sysflash.h"
#include "system/version/version.h"
#include "ui/button/button.h"
#include "ui/front_leds/front_leds.h"
#include "ui/operator_leds/operator_leds.h"
#include "ui/rgb_leds.h"
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

#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(power_sequence, CONFIG_POWER_SEQUENCE_LOG_LEVEL);

// Power supplies turned on in two phases:
// - Phase 1 initializes just enough power supplies for us to use the Operator
// LEDs. It consumes power (150mA), but if the operator places the power switch
// in the off position, then no power is given to the Orb at all, and this is
// preferably what the operators should be doing when not using the Orb.
// - Phase 2 is for turning on all the power supplies. It is gated on the button
// press unless we are booting after a reboot was commanded during an update.

K_THREAD_STACK_DEFINE(reboot_thread_stack, THREAD_STACK_SIZE_POWER_MANAGEMENT);
static struct k_thread reboot_thread_data;

static const struct gpio_dt_spec supply_3v8_enable_rfid_irq_gpio_spec =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), supply_3v8_enable_rfid_irq_gpios);

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

K_SEM_DEFINE(sem_reboot, 0, 1);
static atomic_t reboot_delay_s = ATOMIC_INIT(0);
static k_tid_t reboot_tid = NULL;
static struct gpio_callback shutdown_cb_data;

#define FORMAT_STRING "Checking that %s is ready... "

static int
check_is_ready(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return 1;
    }

    LOG_INF(FORMAT_STRING "yes", name);
    return 0;
}

#define I2C_CLOCK_NODE  DT_PATH(zephyr_user)
#define I2C_CLOCK_CTLR  DT_GPIO_CTLR(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_PIN   DT_GPIO_PIN(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_FLAGS DT_GPIO_FLAGS(I2C_CLOCK_NODE, i2c_clock_gpios)

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

    int ret =
        gpio_pin_configure_dt(&supply_vbat_sw_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return;
    }

    ret = gpio_pin_configure_dt(&supply_5v_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return;
    }

    ret = gpio_pin_configure_dt(&supply_3v3_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
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
    const struct device *i2c_clock = DEVICE_DT_GET(I2C_CLOCK_CTLR);

    Hardware_OrbVersion version = version_get_hardware_rev();

    if (!device_is_ready(supply_vbat_sw_enable_gpio_spec.port) ||
        !device_is_ready(supply_12v_enable_gpio_spec.port) ||
        !device_is_ready(supply_5v_enable_gpio_spec.port) ||
        !device_is_ready(supply_3v8_enable_rfid_irq_gpio_spec.port) ||
        !device_is_ready(supply_3v3_enable_gpio_spec.port) ||
        !device_is_ready(supply_1v8_enable_gpio_spec.port)) {
        return 1;
    }

    // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5
    if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        if (!device_is_ready(supply_3v3_ssd_enable_gpio_spec.port) ||
            !device_is_ready(supply_3v3_wifi_enable_gpio_spec.port)) {
            return 1;
        }
    }

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
        return 1;
    }

    int ret =
        gpio_pin_configure_dt(&supply_vbat_sw_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return 1;
    }

    ret = gpio_pin_configure_dt(&supply_5v_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return 1;
    }

    ret = gpio_pin_configure_dt(&supply_3v3_enable_gpio_spec, GPIO_OUTPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return 1;
    }

    gpio_pin_set_dt(&supply_vbat_sw_enable_gpio_spec, 1);
    LOG_INF("VBAT SW enabled");
    k_msleep(100);

    gpio_pin_set_dt(&supply_5v_enable_gpio_spec, 1);
    LOG_INF("5V enabled");

    k_msleep(100);

    gpio_pin_set_dt(&supply_3v3_enable_gpio_spec, 1);
    LOG_INF("3.3V enabled");

    // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5
    if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        gpio_pin_set_dt(&supply_3v3_ssd_enable_gpio_spec, 1);
        LOG_INF("3.3V SSD power supply enabled");

        gpio_pin_set_dt(&supply_3v3_wifi_enable_gpio_spec, 1);
        LOG_INF("3.3V WIFI power supply enabled");
    }

    k_msleep(100);

    ret = gpio_pin_configure_dt(&supply_12v_enable_gpio_spec, GPIO_OUTPUT);

    if (ret != 0) {
        ASSERT_SOFT(ret);
        return 1;
    }

    gpio_pin_set_dt(&supply_12v_enable_gpio_spec, 1);
    LOG_INF("12V enabled");

    // 3.8V regulator only available on EV1...4
    if ((version == Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
        (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
        (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
        (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
        gpio_pin_set_dt(&supply_3v8_enable_rfid_irq_gpio_spec, 1);
        LOG_INF("3.8V enabled");
    }

    ret = gpio_pin_configure_dt(&supply_1v8_enable_gpio_spec, GPIO_OUTPUT);

    if (ret != 0) {
        ASSERT_SOFT(ret);
        return 1;
    }

    gpio_pin_set_dt(&supply_1v8_enable_gpio_spec, 1);
    LOG_INF("1.8V power supply enabled");

    k_msleep(100);

    return 0;
}

BUILD_ASSERT(CONFIG_I2C_INIT_PRIORITY > SYS_INIT_POWER_SUPPLY_INIT_PRIORITY,
             "I2C must be initialized _after_ the power supplies so that the "
             "safety circuit does't get tripped");

SYS_INIT(power_turn_on_power_supplies, POST_KERNEL,
         SYS_INIT_POWER_SUPPLY_INIT_PRIORITY);

#define BUTTON_PRESS_TIME_MS 600

#define POWER_BUTTON_NODE  DT_PATH(buttons, power_button)
#define POWER_BUTTON_CTLR  DT_GPIO_CTLR(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_PIN   DT_GPIO_PIN(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_FLAGS DT_GPIO_FLAGS(POWER_BUTTON_NODE, gpios)

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
    const struct device *power_button = DEVICE_DT_GET(POWER_BUTTON_CTLR);

    if (IS_ENABLED(CONFIG_INSTA_BOOT)) {
        LOG_INF(
            "INSTA_BOOT enabled -- not waiting for a button press to boot!");
        return 0;
    }

    if (!device_is_ready(power_button)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INVALID_STATE;
    }

    ret = gpio_pin_configure(power_button, POWER_BUTTON_PIN,
                             POWER_BUTTON_FLAGS | GPIO_INPUT);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    const struct gpio_dt_spec supply_meas_enable_spec = GPIO_DT_SPEC_GET(
        DT_PATH(voltage_measurement), supply_voltages_meas_enable_gpios);
    ret = gpio_pin_configure_dt(&supply_meas_enable_spec, GPIO_OUTPUT);
    ASSERT_SOFT(ret);
    ret = gpio_pin_set_dt(&supply_meas_enable_spec, 1);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    k_msleep(1);

    const struct gpio_dt_spec pvcc_in_gpio_spec =
        GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pvcc_voltage_gpios);
    ret = gpio_pin_configure_dt(&pvcc_in_gpio_spec, GPIO_INPUT);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    LOG_INF("Waiting for button press of " TOSTR(BUTTON_PRESS_TIME_MS) "ms");
    uint32_t operator_led_mask = 0;
    const RgbColor white = RGB_WHITE_OPERATOR_LEDS;
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
        if (!gpio_pin_get(power_button, POWER_BUTTON_PIN)) {
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
        operator_leds_blocking_set(&white, operator_led_mask);

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

    LOG_INF("Hello from " CONFIG_BOARD " :)");

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
        LOG_INF("Firmware image not confirmed, confirming");

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

#define SLEEP_WAKE_NODE  DT_PATH(jetson_power_pins, sleep_wake)
#define SLEEP_WAKE_CTLR  DT_GPIO_CTLR(SLEEP_WAKE_NODE, gpios)
#define SLEEP_WAKE_PIN   DT_GPIO_PIN(SLEEP_WAKE_NODE, gpios)
#define SLEEP_WAKE_FLAGS DT_GPIO_FLAGS(SLEEP_WAKE_NODE, gpios)
#define SLEEP            0
#define WAKE             1

#define POWER_ENABLE_NODE  DT_PATH(jetson_power_pins, power_enable)
#define POWER_ENABLE_CTLR  DT_GPIO_CTLR(POWER_ENABLE_NODE, gpios)
#define POWER_ENABLE_PIN   DT_GPIO_PIN(POWER_ENABLE_NODE, gpios)
#define POWER_ENABLE_FLAGS DT_GPIO_FLAGS(POWER_ENABLE_NODE, gpios)
#define ENABLE             1
#define DISABLE            0

#define SYSTEM_RESET_NODE  DT_PATH(jetson_power_pins, system_reset)
#define SYSTEM_RESET_CTLR  DT_GPIO_CTLR(SYSTEM_RESET_NODE, gpios)
#define SYSTEM_RESET_PIN   DT_GPIO_PIN(SYSTEM_RESET_NODE, gpios)
#define SYSTEM_RESET_FLAGS DT_GPIO_FLAGS(SYSTEM_RESET_NODE, gpios)
#define RESET              1
#define OUT_OF_RESET       0

#define SHUTDOWN_REQUEST_NODE DT_PATH(jetson_power_pins, shutdown_request)
#define SHUTDOWN_REQUEST_CTLR DT_GPIO_CTLR(SHUTDOWN_REQUEST_NODE, gpios)

#define LTE_GPS_USB_RESET_NODE  DT_PATH(lte_gps_usb_reset)
#define LTE_GPS_USB_RESET_CTLR  DT_GPIO_CTLR(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_RESET_PIN   DT_GPIO_PIN(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_RESET_FLAGS DT_GPIO_FLAGS(LTE_GPS_USB_RESET_NODE, gpios)
#define LTE_GPS_USB_ON          0

static const struct gpio_dt_spec shutdown_pin =
    GPIO_DT_SPEC_GET_OR(SHUTDOWN_REQUEST_NODE, gpios, {0});
static const struct device *power_enable = DEVICE_DT_GET(SLEEP_WAKE_CTLR);
static const struct device *system_reset = DEVICE_DT_GET(SYSTEM_RESET_CTLR);

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

    if (pins & BIT(shutdown_pin.pin)) {
        gpio_pin_set(power_enable, POWER_ENABLE_PIN, DISABLE);

        // offload reboot to power management thread
        atomic_set(&reboot_delay_s, 1);
        // wake up reboot thread in case already waiting for the reboot
        // this will make the current event take precedence over the
        // currently pending reboot as the reboot thread will now
        // sleep for `reboot_delay_s` second before rebooting
        k_wakeup(reboot_tid);
        k_sem_give(&sem_reboot);

        LOG_INF("Jetson shut down");
    }
}

static void
reboot_thread()
{
    uint32_t delay = 0;

    Hardware_OrbVersion version = version_get_hardware_rev();

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
        RgbColor color = RGB_WHITE;
        operator_leds_set_pattern(
            DistributorLEDsPattern_DistributorRgbLedPattern_PULSING_RGB,
            0b00100, &color);
    }

    do {
        // check if shutdown_pin is active, if so, it means Jetson
        // needs proper shutdown
        if (gpio_pin_get_dt(&shutdown_pin) == 1) {
            // From the Jetson Datasheet DS-10184-001 § 2.6.2 Power Down:
            //  > Once POWER_EN is deasserted, the module will assert
            //  SYS_RESET*, and the baseboard may shut down. SoC 3.3V I/O must
            //  reach 0.5V or lower at most 1.5ms after SYS_RESET* is asserted.
            //  SoC 1.8V I/O must reach 0.5V or lower at most 4ms after
            //  SYS_RESET* is asserted.
            while (gpio_pin_get(system_reset, SYSTEM_RESET_PIN) == 0)
                ;

            gpio_pin_set_dt(&supply_3v3_enable_gpio_spec, 0);
            // Additional control signals for 3V3_SSD and 3V3_WIFI on EV5
            if (version == Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
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

    operator_leds_set_pattern(
        DistributorLEDsPattern_DistributorRgbLedPattern_OFF, 0, NULL);
    front_leds_turn_off_final();

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
shutdown_req_init()
{
    // Jetson is launched, we can now activate shutdown detection
    int ret;

    ret = gpio_pin_configure_dt(&shutdown_pin, GPIO_INPUT);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    ret =
        gpio_pin_interrupt_configure_dt(&shutdown_pin, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&shutdown_cb_data, shutdown_requested,
                       BIT(shutdown_pin.pin));
    ret = gpio_add_callback(shutdown_pin.port, &shutdown_cb_data);
    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

__unused static int
shutdown_req_uninit()
{
    int ret = gpio_pin_interrupt_configure_dt(&shutdown_pin, GPIO_INT_DISABLE);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    ret = gpio_remove_callback(shutdown_pin.port, &shutdown_cb_data);
    if (ret) {
        ASSERT_SOFT(ret);
    }
    return ret;
}

int
boot_turn_on_jetson(void)
{
    const struct device *sleep_wake = DEVICE_DT_GET(SLEEP_WAKE_CTLR);
    const struct device *shutdown_request =
        DEVICE_DT_GET(SHUTDOWN_REQUEST_CTLR);
    const struct device *lte_gps_usb_reset =
        DEVICE_DT_GET(LTE_GPS_USB_RESET_CTLR);
    int ret = 0;

    if (check_is_ready(sleep_wake, "sleep wake pin") ||
        check_is_ready(power_enable, "power enable pin") ||
        check_is_ready(system_reset, "system reset pin") ||
        check_is_ready(shutdown_request, "Shutdown request pin")) {
        return RET_ERROR_INVALID_STATE;
    }

    ret = gpio_pin_configure(power_enable, POWER_ENABLE_PIN,
                             POWER_ENABLE_FLAGS | GPIO_OUTPUT);
    if (ret) {
        ASSERT_SOFT(ret);
    } else {
        LOG_INF("Enabling Jetson power");
        ret = gpio_pin_set(power_enable, POWER_ENABLE_PIN, ENABLE);
        ASSERT_SOFT(ret);

        ret = gpio_pin_configure(system_reset, SYSTEM_RESET_PIN,
                                 SYSTEM_RESET_FLAGS | GPIO_INPUT);
        if (ret) {
            ASSERT_SOFT(ret);
        } else {
            LOG_INF_IMM("Waiting for reset done signal from Jetson");
            while (gpio_pin_get(system_reset, SYSTEM_RESET_PIN) != OUT_OF_RESET)
                ;
            LOG_INF("Reset done");
        }
    }

    ret = gpio_pin_configure(sleep_wake, SLEEP_WAKE_PIN,
                             SLEEP_WAKE_FLAGS | GPIO_OUTPUT);
    if (ret) {
        ASSERT_SOFT(ret);
    } else {
        LOG_INF_IMM("Setting Jetson to WAKE mode");
        ret = gpio_pin_set(sleep_wake, SLEEP_WAKE_PIN, WAKE);
        ASSERT_SOFT(ret);
    }

    ret = gpio_pin_configure(lte_gps_usb_reset, LTE_GPS_USB_RESET_PIN,
                             LTE_GPS_USB_RESET_FLAGS | GPIO_OUTPUT);
    if (ret) {
        ASSERT_SOFT(ret);
    } else {
        LOG_INF("Enabling LTE, GPS, and USB");
        ret = gpio_pin_set(lte_gps_usb_reset, LTE_GPS_USB_RESET_PIN,
                           LTE_GPS_USB_ON);
        ASSERT_SOFT(ret);
    }

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
    int ret;

    if (!device_is_ready(supply_super_cap_enable_gpio_spec.port)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INTERNAL;
    }

    ret =
        gpio_pin_configure_dt(&supply_super_cap_enable_gpio_spec, GPIO_OUTPUT);

    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    gpio_pin_set_dt(&supply_super_cap_enable_gpio_spec, 1);
    LOG_INF("super cap charger enabled");

    k_msleep(1000);
    return RET_SUCCESS;
}

int
boot_turn_off_pvcc(void)
{
    int ret;

    if (!device_is_ready(supply_pvcc_enable_gpio_spec.port)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_pvcc_enable_gpio_spec, GPIO_OUTPUT);

    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    gpio_pin_set_dt(&supply_pvcc_enable_gpio_spec, 0);
    LOG_INF("PVCC disabled");

    return RET_SUCCESS;
}

int
boot_turn_on_pvcc(void)
{
    int ret;

    if (!device_is_ready(supply_pvcc_enable_gpio_spec.port)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        return RET_ERROR_INTERNAL;
    }

    ret = gpio_pin_configure_dt(&supply_pvcc_enable_gpio_spec, GPIO_OUTPUT);

    if (ret != 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

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

ret_code_t
boot_init(const Hardware *hw_version)
{
    // 3.8V regulator only available on EV1...4
    // -> configure pin to output
    // on EV5 and later this pin is an input
    if ((hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV1) ||
        (hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV2) ||
        (hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV3) ||
        (hw_version->version == Hardware_OrbVersion_HW_VERSION_PEARL_EV4)) {
        if (!device_is_ready(supply_3v8_enable_rfid_irq_gpio_spec.port)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            return RET_ERROR_INTERNAL;
        }

        int ret = gpio_pin_configure_dt(&supply_3v8_enable_rfid_irq_gpio_spec,
                                        GPIO_OUTPUT);

        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }

        LOG_INF("EV1...4 Mainboard detected -> SUPPLY_3V8_EN pin configured to "
                "output.");
    } else if (hw_version->version ==
               Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        // On EV5 the signal...
        // UC_ADC_FU_EYE_SAFETY was replaced by 3V3_SSD_SUPPLY_EN_3V3
        // UC_FU_RFID_GPO_3V3 was replaced by 3V3_WIFI_SUPPLY_EN_3V3
        // Both pins must be configured as an output.
        if (!device_is_ready(supply_3v3_ssd_enable_gpio_spec.port)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            return RET_ERROR_INTERNAL;
        }

        if (!device_is_ready(supply_3v3_wifi_enable_gpio_spec.port)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            return RET_ERROR_INTERNAL;
        }

        int ret = gpio_pin_configure_dt(&supply_3v3_ssd_enable_gpio_spec,
                                        GPIO_OUTPUT);

        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }

        ret = gpio_pin_configure_dt(&supply_3v3_wifi_enable_gpio_spec,
                                    GPIO_OUTPUT);

        if (ret != 0) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INTERNAL;
        }

        LOG_INF("EV5 Mainboard detected -> SUPPLY_3V3_SSD_EN and "
                "SUPPLY_3V3_WIFI_EN pins configured to output.");
    }

    return RET_SUCCESS;
}
