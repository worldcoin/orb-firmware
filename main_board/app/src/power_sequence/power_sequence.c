#include "sysflash/sysflash.h"
#include <app_assert.h>
#include <app_config.h>
#include <arch/cpu.h>
#include <assert.h>
#include <bootutil/bootutil.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/regulator.h>
#include <errors.h>
#include <logging/log.h>
#include <stdio.h>
#include <zephyr.h>
LOG_MODULE_REGISTER(power_sequence);

#include "button/button.h"
#include "power_sequence.h"
#include "ui/front_leds/front_leds.h"
#include "ui/operator_leds/operator_leds.h"

// Power supplies turned on in two phases:
// - Phase 1 initializes just enough power supplies for us to use the Operator
// LEDs. It consumes power (150mA), but if the operator places the power switch
// in the off position, then no power is given to the Orb at all, and this is
// preferably what the operators should be doing when not using the Orb.
// - Phase 2 is for turning on all the power supplies. It is gated on the button
// press unless we are booting after a reboot was commanded during an update.

K_THREAD_STACK_DEFINE(reboot_thread_stack, THREAD_STACK_SIZE_POWER_MANAGEMENT);
static struct k_thread reboot_thread_data;

static const struct device *supply_3v3 = DEVICE_DT_GET(DT_PATH(supply_3v3));
static const struct device *supply_1v8 = DEVICE_DT_GET(DT_PATH(supply_1v8));

K_SEM_DEFINE(sem_reboot, 0, 1);
static bool reboot_pending_shutdown_req_line = false;
static uint32_t reboot_delay_s = 0;
static struct gpio_callback shutdown_cb_data;

#define FORMAT_STRING "Checking that %s is ready... "

static int
check_is_ready(const struct device *dev, const char *name)
{
    if (!device_is_ready(dev)) {
        LOG_ERR(FORMAT_STRING "no", name);
        return 1;
    }

    LOG_INF(FORMAT_STRING "yes", name);
    return 0;
}

#define I2C_CLOCK_NODE  DT_PATH(zephyr_user)
#define I2C_CLOCK_CTLR  DT_GPIO_CTLR(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_PIN   DT_GPIO_PIN(I2C_CLOCK_NODE, i2c_clock_gpios)
#define I2C_CLOCK_FLAGS DT_GPIO_FLAGS(I2C_CLOCK_NODE, i2c_clock_gpios)

#ifdef CONFIG_BOARD_MCU_MAIN_V30

#define SUPPLY_5V_PG_NODE  DT_PATH(supply_5v, power_good)
#define SUPPLY_5V_PG_CTLR  DT_GPIO_CTLR(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_PIN   DT_GPIO_PIN(SUPPLY_5V_PG_NODE, gpios)
#define SUPPLY_5V_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_5V_PG_NODE, gpios)

#define SUPPLY_3V3_PG_NODE  DT_PATH(supply_3v3, power_good)
#define SUPPLY_3V3_PG_CTLR  DT_GPIO_CTLR(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_PIN   DT_GPIO_PIN(SUPPLY_3V3_PG_NODE, gpios)
#define SUPPLY_3V3_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_3V3_PG_NODE, gpios)

#define SUPPLY_1V8_PG_NODE  DT_PATH(supply_1v8, power_good)
#define SUPPLY_1V8_PG_CTLR  DT_GPIO_CTLR(SUPPLY_1V8_PG_NODE, gpios)
#define SUPPLY_1V8_PG_PIN   DT_GPIO_PIN(SUPPLY_1V8_PG_NODE, gpios)
#define SUPPLY_1V8_PG_FLAGS DT_GPIO_FLAGS(SUPPLY_1V8_PG_NODE, gpios)

static const struct device *supply_3v3_pg = DEVICE_DT_GET(SUPPLY_3V3_PG_CTLR);

#endif

int
power_turn_on_supplies_phase1(const struct device *dev)
{
    ARG_UNUSED(dev);
    const struct device *vbat_sw_regulator = DEVICE_DT_GET(DT_PATH(vbat_sw));
    const struct device *supply_5v = DEVICE_DT_GET(DT_PATH(supply_5v));

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    const struct device *supply_5v_pg = DEVICE_DT_GET(SUPPLY_5V_PG_CTLR);
#endif

    if (check_is_ready(vbat_sw_regulator, "VBAT SW") ||
        check_is_ready(supply_5v, "5V supply") ||
        check_is_ready(supply_3v3, "3.3V supply")
#if CONFIG_BOARD_MCU_MAIN_V30
        || check_is_ready(supply_3v3_pg, "3.3V supply power good pin") ||
        check_is_ready(supply_5v_pg, "5V supply power good pin")
#endif
    ) {
        return 1;
    }

    regulator_enable(vbat_sw_regulator, NULL);
    LOG_INF("VBAT SW enabled");
    k_msleep(100);

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    if (gpio_pin_configure(supply_5v_pg, SUPPLY_5V_PG_PIN,
                           SUPPLY_5V_PG_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring 5v pg pin!");
        return 1;
    }
#endif

    regulator_enable(supply_5v, NULL);
    LOG_INF("5V power supply enabled");

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on
    // anything else
    while (!gpio_pin_get(supply_5v_pg, SUPPLY_5V_PG_PIN))
        ;
    LOG_INF("5V power supply good");
#else
    k_msleep(100);
#endif

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    if (gpio_pin_configure(supply_3v3_pg, SUPPLY_3V3_PG_PIN,
                           SUPPLY_3V3_PG_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring 3.3v pg pin!");
        return 1;
    }
#endif

    regulator_enable(supply_3v3, NULL);
    LOG_INF("3.3V power supply enabled");
#ifdef CONFIG_BOARD_MCU_MAIN_V30
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on the
    // fan. If we can't turn on the fan, then we don't want to turn on
    // anything else
    while (!gpio_pin_get(supply_3v3_pg, SUPPLY_3V3_PG_PIN))
        ;
    LOG_INF("3.3V power supply good");
#else
    k_msleep(100);
#endif

    return 0;
}

SYS_INIT(power_turn_on_supplies_phase1, POST_KERNEL,
         SYS_INIT_POWER_SUPPLY_PHASE1_PRIORITY);

int
power_turn_on_supplies_phase2(const struct device *dev)
{
    ARG_UNUSED(dev);

    const struct device *supply_12v = DEVICE_DT_GET(DT_PATH(supply_12v));
    const struct device *i2c_clock = DEVICE_DT_GET(I2C_CLOCK_CTLR);
    const struct device *supply_3v8 = DEVICE_DT_GET(DT_PATH(supply_3v8));

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    const struct device *supply_1v8_pg = DEVICE_DT_GET(SUPPLY_1V8_PG_CTLR);
#endif

    if (check_is_ready(supply_12v, "12V supply") ||
        check_is_ready(supply_3v8, "3.8V supply") ||
        check_is_ready(supply_1v8, "1.8V supply")
#ifdef CONFIG_BOARD_MCU_MAIN_V30
        || check_is_ready(supply_1v8_pg, "1.8V supply power good pin")
#endif
    ) {
        return 1;
    }

    regulator_enable(supply_3v3, NULL);
    LOG_INF("3.3V power supply re-enabled");
#ifdef CONFIG_BOARD_MCU_MAIN_V30
    LOG_INF("Waiting on power good...");
    // Wait forever, because if we can't enable this then we can't turn on the
    // fan. If we can't turn on the fan, then we don't want to turn on
    // anything else
    while (!gpio_pin_get(supply_3v3_pg, SUPPLY_3V3_PG_PIN))
        ;
    LOG_INF("3.3V power supply good");
#else
    k_msleep(100);
#endif

    // We configure this pin here before we enable 3.3v supply
    // just so that we can disable the automatically-enabled pull-up.
    // We must do this because providing a voltage to the 3.3v power supply
    // output before it is online can trigger the safety circuit.
    //
    // After this is configured, the I2C initialization will run and
    // re-configure this pin as SCL.
    if (gpio_pin_configure(i2c_clock, I2C_CLOCK_PIN,
                           GPIO_OUTPUT | I2C_CLOCK_FLAGS)) {
        LOG_ERR("Error configuring I2C clock pin!");
        return 1;
    }

    regulator_enable(supply_12v, NULL);
    LOG_INF("12V power supply enabled");

    regulator_enable(supply_3v8, NULL);
    LOG_INF("3.8V power supply enabled");

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    if (gpio_pin_configure(supply_1v8_pg, SUPPLY_1V8_PG_PIN,
                           SUPPLY_1V8_PG_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring 1.8 pg pin!");
        return 1;
    }
#endif

    regulator_enable(supply_1v8, NULL);
    LOG_INF("1.8V power supply enabled");

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    LOG_INF("Waiting on power good...");
    while (!gpio_pin_get(supply_1v8_pg, SUPPLY_1V8_PG_PIN))
        ;
    LOG_INF("1.8V power supply good");
#else
    k_msleep(100);
#endif

    return 0;
}

SYS_INIT(power_turn_on_supplies_phase2, POST_KERNEL,
         SYS_INIT_POWER_SUPPLY_PHASE2_PRIORITY);

#define BUTTON_PRESS_TIME_MS    1500
#define BUTTON_SAMPLE_PERIOD_MS 10

#define POWER_BUTTON_NODE  DT_PATH(buttons, power_button)
#define POWER_BUTTON_CTLR  DT_GPIO_CTLR(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_PIN   DT_GPIO_PIN(POWER_BUTTON_NODE, gpios)
#define POWER_BUTTON_FLAGS DT_GPIO_FLAGS(POWER_BUTTON_NODE, gpios)

static int
power_wait_for_power_button_press(void)
{
    const struct device *power_button = DEVICE_DT_GET(POWER_BUTTON_CTLR);

    if (!device_is_ready(power_button)) {
        LOG_ERR("power button is not ready!");
        return 1;
    }

    if (gpio_pin_configure(power_button, POWER_BUTTON_PIN,
                           POWER_BUTTON_FLAGS | GPIO_INPUT)) {
        LOG_ERR("Error configuring power button!");
        return 1;
    }

    LOG_INF("Waiting for button press of " TOSTR(BUTTON_PRESS_TIME_MS) "ms");
    for (size_t i = 0; i < (BUTTON_PRESS_TIME_MS / BUTTON_SAMPLE_PERIOD_MS);
         ++i) {
        if (!gpio_pin_get(power_button, POWER_BUTTON_PIN)) {
            if (i > 1) {
                LOG_INF("Press stopped.");
            }
            i = 0;
        }
        if (i == 1) {
            LOG_INF("Press started.");
        }
        k_msleep(BUTTON_SAMPLE_PERIOD_MS);
    }

    return 0;
}

/**
 * Decide whether to wait for user to press the button to start the Orb
 * or to directly boot the Orb (after fresh update)
 * @param dev
 * @return error code
 */
int
app_init_state(const struct device *dev)
{
    ARG_UNUSED(dev);

    int ret = 0;

    // disable now that we have initialized operator LED
    // see SYS_INIT #52
    regulator_disable(supply_3v3);

    LOG_INF("Hello from " CONFIG_BOARD " :)");

    // read image status to know whether we are waiting for user to press
    // the button
    struct boot_swap_state primary_slot = {0};
    boot_read_swap_state_by_id(FLASH_AREA_IMAGE_PRIMARY(0), &primary_slot);

    LOG_DBG("Magic: %u, swap type: %u, image_ok: %u", primary_slot.magic,
            primary_slot.swap_type, primary_slot.image_ok);

    // if FW image is confirmed, gate turning on power supplies on button press
    // otherwise, application have been updated and not confirmed, boot Jetson
    if (primary_slot.image_ok != BOOT_FLAG_UNSET ||
        primary_slot.magic == BOOT_MAGIC_UNSET) {
        ret = power_wait_for_power_button_press();
    } else {
        LOG_INF("Firmware image not confirmed");
    }
    LOG_INF("Booting system...");

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

#define SYSTEM_RESET_UI_DELAY 200

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

        if (reboot_pending_shutdown_req_line) {
            // offload reboot to power management thread
            reboot_delay_s = 1;
            k_sem_give(&sem_reboot);
        } else {
            LOG_INF("Jetson shut down");
        }
    }
}

static void
reboot_thread()
{
    k_sem_take(&sem_reboot, K_FOREVER);

    if (reboot_pending_shutdown_req_line) {
        // From the Jetson Datasheet DS-10184-001 § 2.6.2 Power Down:
        //  > Once POWER_EN is deasserted, the module will assert SYS_RESET*,
        //  and the baseboard may shut down. SoC 3.3V I/O must reach 0.5V or
        //  lower at most 1.5ms after SYS_RESET* is asserted. SoC 1.8V I/O must
        //  reach 0.5V or lower at most 4ms after SYS_RESET* is asserted.
        while (gpio_pin_get(system_reset, SYSTEM_RESET_PIN) != RESET)
            ;

        regulator_disable(supply_3v3);
        regulator_disable(supply_1v8);

        // the jetson has been turned off following the specs, we can now
        // wait `reboot_delay_s` to reset
    }

    LOG_INF("Rebooting in %u seconds", reboot_delay_s);

    struct boot_swap_state secondary_slot = {0};
    boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SECONDARY(0), &secondary_slot);
    LOG_DBG("Secondary Magic: %u, swap type: %u, image_ok: %u",
            secondary_slot.magic, secondary_slot.swap_type,
            secondary_slot.image_ok);

    if (reboot_delay_s > 0) {
        k_msleep(reboot_delay_s * 1000 - SYSTEM_RESET_UI_DELAY);
    }

    // check if a new firmware image is about to be installed
    // turn on center LEDs in white during update
    // otherwise turn UI off so that re-enabling regulators during boot
    // doesn't make the LEDs blink with previous configuration
    if (secondary_slot.magic == BOOT_MAGIC_GOOD) {
        front_leds_set_pattern(
            UserLEDsPattern_UserRgbLedPattern_ALL_WHITE_ONLY_CENTER, 0, 0,
            NULL);
    } else {
        front_leds_set_pattern(UserLEDsPattern_UserRgbLedPattern_OFF, 0, 0,
                               NULL);
    }
    operator_leds_set_pattern(
        DistributorLEDsPattern_DistributorRgbLedPattern_OFF, 0, NULL);
    k_msleep(SYSTEM_RESET_UI_DELAY);

    NVIC_SystemReset();
}

static int
shutdown_req_init()
{
    // Jetson is launched, we can now activate shutdown detection
    int ret;

    ret = gpio_pin_configure_dt(&shutdown_pin, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret,
                shutdown_pin.port->name, shutdown_pin.pin);
        return RET_ERROR_INTERNAL;
    }

    ret =
        gpio_pin_interrupt_configure_dt(&shutdown_pin, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret,
                shutdown_pin.port->name, shutdown_pin.pin);
        return RET_ERROR_INTERNAL;
    }

    gpio_init_callback(&shutdown_cb_data, shutdown_requested,
                       BIT(shutdown_pin.pin));
    ret = gpio_add_callback(shutdown_pin.port, &shutdown_cb_data);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to add callback on %s pin %d", ret,
                shutdown_pin.port->name, shutdown_pin.pin);
        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

__unused static int
shutdown_req_uninit()
{
    int ret = gpio_pin_interrupt_configure_dt(&shutdown_pin, GPIO_INT_DISABLE);
    if (ret) {
        LOG_ERR("Error disabling shutdown req interrupt");
        return ret;
    }

    ret = gpio_remove_callback(shutdown_pin.port, &shutdown_cb_data);
    if (ret) {
        LOG_ERR("Error removing shutdown req interrupt");
    }
    return ret;
}

int
power_turn_on_jetson(void)
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
            LOG_INF("Waiting for reset done signal from Jetson");
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
        LOG_INF("Setting Jetson to WAKE mode");
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

#ifdef CONFIG_BOARD_MCU_MAIN_V31
    // mainboard 3.0 uses PC13 and PE13 for shutdown request line and power
    // button, so we enable interrupt on shutdown line only when necessary, see
    // power_reboot_set_pending()
    shutdown_req_init();
#endif

    // Spawn reboot thread
    k_thread_create(&reboot_thread_data, reboot_thread_stack,
                    K_THREAD_STACK_SIZEOF(reboot_thread_stack), reboot_thread,
                    NULL, NULL, NULL, THREAD_PRIORITY_POWER_MANAGEMENT, 0,
                    K_NO_WAIT);

    return RET_SUCCESS;
}

int
power_turn_on_super_cap_charger(void)
{
    int ret;

    const struct device *supply_super_cap =
        DEVICE_DT_GET(DT_PATH(supply_super_cap));
    if (check_is_ready(supply_super_cap, "super cap charger")) {
        return RET_ERROR_INVALID_STATE;
    }

    ret = regulator_enable(supply_super_cap, NULL);
    if (ret < 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    LOG_INF("super cap charger enabled");
    k_msleep(1000);
    return RET_SUCCESS;
}

int
power_turn_on_pvcc(void)
{
    int ret;
    const struct device *supply_pvcc = DEVICE_DT_GET(DT_PATH(supply_pvcc));
    if (check_is_ready(supply_pvcc, "pvcc supply")) {
        return RET_ERROR_INVALID_STATE;
    }

    ret = regulator_enable(supply_pvcc, NULL);
    if (ret < 0) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

    LOG_INF("pvcc supply enabled");
    return RET_SUCCESS;
}

int
power_reset(uint32_t delay_s)
{
    if (reboot_delay_s) {
        // already in progress
        return RET_ERROR_INVALID_STATE;
    } else {
        power_reboot_clear_pending();
        reboot_delay_s = delay_s;

        k_sem_give(&sem_reboot);
    }

    return RET_SUCCESS;
}

void
power_reboot_set_pending(void)
{
#ifdef CONFIG_BOARD_MCU_MAIN_V30
    // uninit button on GPIOC13 to allow enabling GPIOE13 interrupt
    button_uninit();
    shutdown_req_init();
#endif

    reboot_pending_shutdown_req_line = true;
}

void
power_reboot_clear_pending(void)
{
    reboot_pending_shutdown_req_line = false;

#ifdef CONFIG_BOARD_MCU_MAIN_V30
    shutdown_req_uninit();
    button_init();
#endif
}
