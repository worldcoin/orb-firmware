if (CONFIG_ORB_MCU_LIBRARIES_LIB)
    # publicly available
    zephyr_include_directories(include)

    # add specific libraries depending on configuration
    add_subdirectory_ifdef(CONFIG_ORB_LIB_ERRORS errors)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_DFU dfu)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_CAN_MESSAGING can_messaging)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_UART_MESSAGING uart_messaging)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_HEALTH_MONITORING health_monitoring)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_LOG_BACKEND_CAN log_backend_can)
    add_subdirectory_ifdef(CONFIG_ORB_I2C_SCAN i2c_scan)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_STORAGE storage)
endif ()
