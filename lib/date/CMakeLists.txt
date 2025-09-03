if (CONFIG_ORB_LIB_DATE)

    list(APPEND SRC_FILES date.c)

    orb_library(
        ${SRC_FILES}
    )

endif ()
