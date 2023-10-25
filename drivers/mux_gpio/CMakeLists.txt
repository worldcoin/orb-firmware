# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_I2C_MUX_GPIO i2c_mux_gpio.c)
zephyr_library_sources_ifdef(CONFIG_SPI_MUX_GPIO spi_mux_gpio.c)
