#include "orb_logs.h"
#include "system/diag.h"
#include <app_assert.h>
#include <errors.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(sound, CONFIG_SOUND_LOG_LEVEL);

/**
 * Driver for TAS5805M audio amplifier
 */

#define SOUND_AMP_MUX_NODE DT_PATH(zephyr_user)
#define SOUND_AMP_MUX_CTLR DT_GPIO_CTLR(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)
#define SOUND_AMP_MUX_PIN  DT_GPIO_PIN(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)
#define SOUND_AMP_MUX_FLAGS                                                    \
    DT_GPIO_FLAGS(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)

#define LEVEL_SHIFTER_EN_NODE DT_PATH(zephyr_user)
#define LEVEL_SHIFTER_EN_CTLR                                                  \
    DT_GPIO_CTLR(LEVEL_SHIFTER_EN_NODE, level_shifter_enable_gpios)
#define LEVEL_SHIFTER_EN_PIN                                                   \
    DT_GPIO_PIN(LEVEL_SHIFTER_EN_NODE, level_shifter_enable_gpios)
#define LEVEL_SHIFTER_EN_FLAGS                                                 \
    DT_GPIO_FLAGS(LEVEL_SHIFTER_EN_NODE, level_shifter_enable_gpios)

#define MCU    1
#define JETSON 0

#define SOUND_AMP_I2C        DT_NODELABEL(i2c1)
#define SOUND_AMP_ADDR       0x2c
#define SOUND_AMP_REG_CTRL2  0x3
#define SOUND_AMP_REG_AGAIN  0x54
#define SOUND_AMP_REG_DIE_ID 0x67

#define ANALOG_GAIN_MINUS_3DB 6

int
sound_init(void)
{
    int err_code = 0;
    uint8_t die_id = 0xFF;

    const struct device *level_shifter_en =
        DEVICE_DT_GET(LEVEL_SHIFTER_EN_CTLR);

    if (!device_is_ready(level_shifter_en)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        err_code = RET_ERROR_INVALID_STATE;
    } else {
        err_code = gpio_pin_configure(level_shifter_en, LEVEL_SHIFTER_EN_PIN,
                                      LEVEL_SHIFTER_EN_FLAGS | GPIO_OUTPUT);
        ASSERT_SOFT(err_code);
        if (err_code == 0) {
            err_code = gpio_pin_set(level_shifter_en, LEVEL_SHIFTER_EN_PIN, 1);
            ASSERT_SOFT(err_code);
        }
    }

    const struct device *sound_mux = DEVICE_DT_GET(SOUND_AMP_MUX_CTLR);
    const struct device *sound_i2c = DEVICE_DT_GET(SOUND_AMP_I2C);

    if (err_code == 0) {
        if (!device_is_ready(sound_mux)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            err_code = RET_ERROR_INVALID_STATE;
        } else {
            err_code = gpio_pin_configure(sound_mux, SOUND_AMP_MUX_PIN,
                                          SOUND_AMP_MUX_FLAGS | GPIO_OUTPUT);
            ASSERT_SOFT(err_code);
            if (err_code == 0) {
                err_code = gpio_pin_set(sound_mux, SOUND_AMP_MUX_PIN, JETSON);
                ASSERT_SOFT(err_code);
            }
        }
    }

    if (err_code == 0) {
        if (!device_is_ready(sound_i2c)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            err_code = RET_ERROR_INVALID_STATE;
        } else {
            err_code = i2c_reg_read_byte(sound_i2c, SOUND_AMP_ADDR,
                                         SOUND_AMP_REG_DIE_ID, &die_id);
            ASSERT_SOFT(err_code);
            if (err_code == 0) {
                err_code = i2c_reg_write_byte(sound_i2c, SOUND_AMP_ADDR,
                                              SOUND_AMP_REG_AGAIN,
                                              ANALOG_GAIN_MINUS_3DB);
                ASSERT_SOFT(err_code);
                if (err_code == 0) {
                    err_code = i2c_reg_write_byte(sound_i2c, SOUND_AMP_ADDR,
                                                  SOUND_AMP_REG_CTRL2, 0x03);
                    ASSERT_SOFT(err_code);
                }
            }
        }
    }

    if (err_code || die_id != 0x00) {
        diag_set_status(
            orb_mcu_HardwareDiagnostic_Source_UI_SOUND,
            orb_mcu_HardwareDiagnostic_Status_STATUS_INITIALIZATION_ERROR);
    } else {
        diag_set_status(orb_mcu_HardwareDiagnostic_Source_UI_SOUND,
                        orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    }

    return err_code;
}
