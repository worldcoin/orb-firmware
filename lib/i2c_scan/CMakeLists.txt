zephyr_sources_ifdef(CONFIG_ORB_I2C_SCAN
        i2c_scan.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_I2C_SCAN include)
