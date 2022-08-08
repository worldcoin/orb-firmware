if (CONFIG_ORB_LIB_CAN_MESSAGE)
    orb_library(
        can_messaging.c
        canbus_rx.c
        canbus_rx_isotp.c
        canbus_tx.c
        canbus_tx_isotp.c
    )
endif ()
