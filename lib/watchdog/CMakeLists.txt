if ((CONFIG_ORB_LIB_WATCHDOG) AND (CONFIG_BOARD_PEARL_MAIN OR CONFIG_BOARD_DIAMOND_MAIN))

    list(APPEND SRC_FILES watchdog.c)

    orb_library(
        ${SRC_FILES}
    )

endif ()
