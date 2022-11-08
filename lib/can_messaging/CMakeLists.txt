if (CONFIG_ORB_LIB_CAN_MESSAGING)
    if(CONFIG_ORB_LIB_CAN_MESSAGING AND CONFIG_ISOTP)
        set(ISOTP_SRCS
            canbus_rx_isotp.c
            canbus_tx_isotp.c
            )
    endif()

    orb_library(
        can_messaging.c
        canbus_rx.c
        canbus_tx.c
        ${ISOTP_SRCS}
    )
endif ()
