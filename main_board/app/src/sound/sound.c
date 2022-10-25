#include <app_assert.h>
#include <errors.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sound, CONFIG_SOUND_LOG_LEVEL);

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

#define SOUND_AMP_I2C       DT_NODELABEL(i2c1)
#define SOUND_AMP_ADDR      0x2c
#define SOUND_AMP_REG_CTRL2 0x3

int
sound_init(void)
{
    int err_code = 0;

    const struct device *level_shifter_en =
        DEVICE_DT_GET(LEVEL_SHIFTER_EN_CTLR);

    if (!device_is_ready(level_shifter_en)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
    } else {
        err_code = gpio_pin_configure(level_shifter_en, LEVEL_SHIFTER_EN_PIN,
                                      LEVEL_SHIFTER_EN_FLAGS | GPIO_OUTPUT);
        if (err_code) {
            ASSERT_SOFT(RET_ERROR_INTERNAL);
        } else {
            err_code = gpio_pin_set(level_shifter_en, LEVEL_SHIFTER_EN_PIN, 1);
            ASSERT_SOFT(err_code);
        }
    }

    const struct device *sound_mux = DEVICE_DT_GET(SOUND_AMP_MUX_CTLR);
    const struct device *sound_i2c = DEVICE_DT_GET(SOUND_AMP_I2C);

    if (!device_is_ready(sound_mux)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
    } else {
        err_code = gpio_pin_configure(sound_mux, SOUND_AMP_MUX_PIN,
                                      SOUND_AMP_MUX_FLAGS | GPIO_OUTPUT);
        if (err_code) {
            ASSERT_SOFT(err_code);
        } else {
            err_code = gpio_pin_set(sound_mux, SOUND_AMP_MUX_PIN, JETSON);
            ASSERT_SOFT(err_code);
        }
    }

    if (!device_is_ready(sound_i2c)) {
        ASSERT_SOFT(RET_ERROR_INVALID_STATE);
    } else {
        LOG_INF("Giving control of sound amp to Jetson");
        i2c_reg_write_byte(sound_i2c, SOUND_AMP_ADDR, SOUND_AMP_REG_CTRL2,
                           0x03);
    }

    return RET_SUCCESS;
}
