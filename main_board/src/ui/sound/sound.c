#include "orb_logs.h"
#include "system/diag.h"
#include <app_assert.h>
#include <errors.h>
#include <orb_state.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(sound, CONFIG_SOUND_LOG_LEVEL);
ORB_STATE_REGISTER(sound);

/**
 * Driver for TAS5805M audio amplifier
 */

#define GPIO_OUTPUT_MUX_MCU    1
#define GPIO_OUTPUT_MUX_JETSON 0

#define SOUND_AMP_REG_CTRL2       0x3
#define SOUND_AMP_REG_DIG_VOL_CTL 0x4C
#define SOUND_AMP_REG_AGAIN       0x54
#define SOUND_AMP_REG_DIE_ID      0x67

// Attenuation value in dB in increments of 0.5 from 0 to 15.5
#define ANALOG_GAIN_ATTENUATION_DB 3

BUILD_ASSERT(ANALOG_GAIN_ATTENUATION_DB >= 0 &&
                 ANALOG_GAIN_ATTENUATION_DB <= 15.5,
             "ANALOG_GAIN_ATTENUATION_DB out of range!");

// Digital Volume
// These bits control both left and right channel digital volume. The
// digital volume is 24 dB to -103 dB (127 dB diff) in -0.5 dB step.
#define DIGITAL_VOL_DB           (-10) // -10 dB
#define DIGITAL_VOL_DB_REG_VALUE MIN(((unsigned)(24 - DIGITAL_VOL_DB) * 2), 255)
BUILD_ASSERT(DIGITAL_VOL_DB >= -103 && DIGITAL_VOL_DB <= 24,
             "DIGITAL_VOL_DB out of range!");

const struct i2c_dt_spec sound_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(audio_amp));
const struct gpio_dt_spec sound_mux =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), sound_amp_mux_gpios);
const struct gpio_dt_spec level_shifter_en =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), level_shifter_enable_gpios);

// The AGAIN register value may range from 0-31, where
//  1 =  -0.5dB
//  2 =  -1.0dB
//  3 =  -1.5dB
// ...
// 31 = -15.5dB
#define ANALOG_GAIN_ATTENUATION_DB_REG_VALUE                                   \
    MIN(((unsigned)(ANALOG_GAIN_ATTENUATION_DB * 2)), 31)

int
sound_init(const orb_mcu_Hardware *const hw)
{
    int err_code = 0;
    uint8_t die_id = 0xFF;
    uint8_t value;

    if (!device_is_ready(level_shifter_en.port)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
        err_code = RET_ERROR_INVALID_STATE;
    } else {
        err_code = gpio_pin_configure_dt(&level_shifter_en, GPIO_OUTPUT_ACTIVE);
        ASSERT_SOFT(err_code);
    }

    /* redirect sound amplifier input to Jetson */
    if (err_code == 0) {
        if (!device_is_ready(sound_mux.port)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            err_code = RET_ERROR_INVALID_STATE;
        } else {
            err_code = gpio_pin_configure_dt(&sound_mux, GPIO_OUTPUT);
            ASSERT_SOFT(err_code);
            if (err_code == 0) {
                err_code = gpio_pin_set_dt(&sound_mux, GPIO_OUTPUT_MUX_JETSON);
                ASSERT_SOFT(err_code);
            }
        }
    }

    if (err_code == 0) {
        if (!device_is_ready(sound_i2c.bus)) {
            ASSERT_SOFT(RET_ERROR_INVALID_STATE);
            err_code = RET_ERROR_INVALID_STATE;
        } else {
            err_code =
                i2c_reg_read_byte_dt(&sound_i2c, SOUND_AMP_REG_DIE_ID, &die_id);
            ASSERT_SOFT(err_code);

            if (hw->version >=
                orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_6) {
                if (err_code == 0) {
                    LOG_INF("Setting digital vol to %.1fdB",
                            (double)DIGITAL_VOL_DB);
                    err_code = i2c_reg_write_byte_dt(&sound_i2c,
                                                     SOUND_AMP_REG_DIG_VOL_CTL,
                                                     DIGITAL_VOL_DB_REG_VALUE);
                    ASSERT_SOFT(err_code);

                    value = 0;
                    err_code = i2c_reg_read_byte_dt(
                        &sound_i2c, SOUND_AMP_REG_DIG_VOL_CTL, &value);
                    ASSERT_SOFT(err_code);
                    if (value != DIGITAL_VOL_DB_REG_VALUE) {
                        LOG_ERR("Read back digital volume (%u) is different "
                                "from the one "
                                "set (%u)",
                                value, DIGITAL_VOL_DB_REG_VALUE);
                    }
                }
            }

            if (err_code == 0) {
                LOG_INF("Setting audio amp attenuation to %.1fdB",
                        (double)ANALOG_GAIN_ATTENUATION_DB);
                err_code =
                    i2c_reg_write_byte_dt(&sound_i2c, SOUND_AMP_REG_AGAIN,
                                          ANALOG_GAIN_ATTENUATION_DB_REG_VALUE);
                ASSERT_SOFT(err_code);

                value = 0;
                err_code = i2c_reg_read_byte_dt(&sound_i2c, SOUND_AMP_REG_AGAIN,
                                                &value);
                ASSERT_SOFT(err_code);
                if (value != ANALOG_GAIN_ATTENUATION_DB_REG_VALUE) {
                    LOG_ERR(
                        "Read back attenuation (%u) is different from the one "
                        "set (%u)",
                        value, ANALOG_GAIN_ATTENUATION_DB);
                }

                if (err_code == 0) {
                    err_code = i2c_reg_write_byte_dt(&sound_i2c,
                                                     SOUND_AMP_REG_CTRL2, 0x03);
                    ASSERT_SOFT(err_code);
                }
            }
        }
    }

    if (err_code || die_id != 0x00) {
        ORB_STATE_SET_CURRENT(RET_ERROR_NOT_INITIALIZED,
                              "comm issue (%d), die id: %d", err_code, die_id);
    } else {
        ORB_STATE_SET_CURRENT(RET_SUCCESS);
    }

    return err_code;
}
