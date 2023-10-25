/*
 * Copyright (c) 2023 Tools for Humanity
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT we_spi_rgb_led

#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

struct spi_rgb_led_config {
    struct spi_dt_spec bus;
    size_t num_leds;
};

static int
spi_rgb_led_update(const struct device *dev, void *buf, size_t size)
{
    const struct spi_rgb_led_config *config = dev->config;
    static const uint8_t zeros[] = {0, 0, 0, 0};

    uint8_t ones[((size / sizeof(struct led_rgb)) / 8 / 2) + 1];
    memset(ones, 0xFF, sizeof ones);

    const struct spi_buf tx_bufs[] = {
        {
            /* Start frame: at least 32 zeros */
            .buf = (uint8_t *)zeros,
            .len = sizeof(zeros),
        },
        {
            /* LED data itself */
            .buf = buf,
            .len = size,
        },
        {
            /* End frame: at least (size/sizeof(struct led_rgb))/2 ones
             * to clock the remaining bits to the LEDs at the end of
             * the strip.
             */
            .buf = (uint8_t *)ones,
            .len = sizeof(ones),
        },
    };
    const struct spi_buf_set tx = {.buffers = tx_bufs,
                                   .count = ARRAY_SIZE(tx_bufs)};

    return spi_write_dt(&config->bus, &tx);
}

static int
spi_rgb_led_update_rgb(const struct device *dev, struct led_rgb *pixels,
                       size_t count)
{
    uint8_t *p = (uint8_t *)pixels;
    size_t i;

    /* Make sure we can write to the pixels array */
    BUILD_ASSERT(sizeof(struct led_rgb) == 4, "led_rgb is not 4 bytes long: "
                                              "cannot write to pixels array");

    /* Rewrite to the on-wire format:
     * Flags (3 bits) | Dimming (5 bits) | Green (8 bits) | Blue (8 bits) | Red
     * (8 bits)
     * */
    for (i = 0; i < count; i++) {
        uint8_t prefix = 0;

        /*
         * SOF (3 bits) followed by the 0 to 31 global dimming level
         *
         * If the global dimming level is zero, we need to send a
         * special prefix byte to the LED strip.
         *
         * FIXME: issues with sleep mode: some LEDs aren't waking up
         * From the doc, we might have to wait 1ms before sending the
         * RGB data and after sending the flags:
         * > To activate the LED after the sleep modus the Flag should
         * > be equal to [3x1] bits and the Dimming frame is different
         * > than 5b00000 (the estimated time for the LED to wake up is
         * > about 1ms).
         */
#if defined(CONFIG_SPI_RGB_LED_DIMMING)
        if (pixels[i].scratch == 0) {
            prefix = 0xA0;
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
        } else {
            prefix = 0xE0 | (pixels[i].scratch & 0x1F);
        }
#else
        /* When CONFIG_SPI_RGB_LED_DIMMING is not enabled, we always
         * send the maximum global dimming level, the RGB values are
         * going to do the dimming.
         */
        if (pixels[i].r || pixels[i].g || pixels[i].b) {
            prefix = 0xE0 | 0x1F;
        } else {
            prefix = 0xA0;
        }
#endif

        uint8_t r = pixels[i].r;
        uint8_t g = pixels[i].g;
        uint8_t b = pixels[i].b;

        *p++ = prefix;
        *p++ = g;
        *p++ = b;
        *p++ = r;
    }

    return spi_rgb_led_update(dev, pixels, sizeof(struct led_rgb) * count);
}

static int
spi_rgb_led_update_channels(const struct device *dev, uint8_t *channels,
                            size_t num_channels)
{
    UNUSED(dev);
    UNUSED(channels);
    UNUSED(num_channels);

    /* Not implemented */
    return -EINVAL;
}

static int
spi_rgb_led_init(const struct device *dev)
{
    const struct spi_rgb_led_config *config = dev->config;

    if (!spi_is_ready_dt(&config->bus)) {
        return -ENODEV;
    }

    return 0;
}

static const struct led_strip_driver_api spi_rgb_led_api = {
    .update_rgb = spi_rgb_led_update_rgb,
    .update_channels = spi_rgb_led_update_channels,
};

#define SPI_RGB_LED_DEVICE(idx)                                                \
    static const struct spi_rgb_led_config spi_rgb_led_##idx##_config = {      \
        .bus = SPI_DT_SPEC_INST_GET(                                           \
            idx, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0)}; \
                                                                               \
    DEVICE_DT_INST_DEFINE(idx, spi_rgb_led_init, NULL, NULL,                   \
                          &spi_rgb_led_##idx##_config, POST_KERNEL,            \
                          CONFIG_LED_STRIP_INIT_PRIORITY, &spi_rgb_led_api);

DT_INST_FOREACH_STATUS_OKAY(SPI_RGB_LED_DEVICE)

BUILD_ASSERT(CONFIG_SPI_INIT_PRIORITY < CONFIG_LED_STRIP_INIT_PRIORITY,
             "initialize SPI before LED strip");
