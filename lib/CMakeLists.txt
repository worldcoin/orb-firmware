if (CONFIG_ORB_MCU_LIBRARIES_LIB)
    # workspace-wide header files
    zephyr_include_directories(include)

    # add specific libraries depending on configuration
    add_subdirectory_ifdef(CONFIG_ORB_LIB_ERRORS errors)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_DFU dfu)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_CAN_MCU_MESSAGE can_mcu_message)
    add_subdirectory_ifdef(CONFIG_ORB_LIB_HEALTH_MONITORING health_monitoring)
    add_subdirectory_ifdef(CONFIG_ORB_I2C_SCAN i2c_scan)
endif ()
