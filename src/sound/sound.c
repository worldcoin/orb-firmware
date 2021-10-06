#include <device.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(sound);

#define SOUND_AMP_MUX_NODE DT_PATH(zephyr_user)
#define SOUND_AMP_MUX_CTLR DT_GPIO_CTLR(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)
#define SOUND_AMP_MUX_PIN DT_GPIO_PIN(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)
#define SOUND_AMP_MUX_FLAGS                                                    \
    DT_GPIO_FLAGS(SOUND_AMP_MUX_NODE, sound_amp_mux_gpios)

#define MCU 1
#define JETSON 0

#define SOUND_AMP_I2C DT_NODELABEL(i2c1)
#define SOUND_AMP_ADDR 0x2c
#define SOUND_AMP_REG_CTRL2 0x3

int init_sound(void)
{
    const struct device *sound_mux = DEVICE_DT_GET(SOUND_AMP_MUX_CTLR);
    const struct device *sound_i2c = DEVICE_DT_GET(SOUND_AMP_I2C);

    if (!device_is_ready(sound_mux)) {
        LOG_ERR("Sound mux is not ready!");
        return 1;
    }

    if (!device_is_ready(sound_i2c)) {
        LOG_ERR("Sound i2c is not ready!");
        return 1;
    }

    if (gpio_pin_configure(sound_mux, SOUND_AMP_MUX_PIN,
                           SOUND_AMP_MUX_FLAGS | GPIO_OUTPUT)) {
        LOG_ERR("Error configuring sound amp mux pin!");
        return 1;
    }

    gpio_pin_set(sound_mux, SOUND_AMP_MUX_PIN, JETSON);

    LOG_INF("Giving control of sound amp to Jetson");
    i2c_reg_write_byte(sound_i2c, SOUND_AMP_ADDR, SOUND_AMP_REG_CTRL2, 0x03);

    return 0;
}
