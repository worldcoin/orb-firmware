zephyr_sources_ifdef(CONFIG_ORB_LIB_CAN_MCU_MESSAGE
        can_messaging.c
        canbus_rx.c
        canbus_rx_isotp.c
        canbus_tx.c
        canbus_tx_isotp.c
        )
zephyr_include_directories_ifdef(CONFIG_ORB_LIB_CAN_MCU_MESSAGE include)
