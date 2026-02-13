add_subdirectory_ifdef(CONFIG_LED_STRIP led_strip)
add_subdirectory_ifdef(CONFIG_SENSOR sensor)
add_subdirectory_ifdef(CONFIG_SC18IS606 i2c_spi_bridge)
add_subdirectory_ifdef(CONFIG_LM51XX_REGULATOR regulator)

if (CONFIG_SPI_MUX_GPIO OR CONFIG_I2C_MUX_GPIO)
add_subdirectory(mux_gpio)
endif ()
