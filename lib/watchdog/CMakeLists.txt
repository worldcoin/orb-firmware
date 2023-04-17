if (CONFIG_ORB_LIB_WATCHDOG)

    list(APPEND SRC_FILES watchdog.c)

    orb_library(
        ${SRC_FILES}
    )

endif ()
