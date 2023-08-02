if (CONFIG_ORB_LIB_LOGS_CAN)
    if (CONFIG_ORB_LIB_LOG_BACKEND_CAN)
        set(OPT_SRCS
            log_backend_can.c
            )
    endif ()

    orb_library(
        logs_can.c
        ${OPT_SRCS}
    )
endif ()
