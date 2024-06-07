if (CONFIG_ORB_LIB_HEALTH_MONITORING)
    set (SRC_FILES heartbeat.c)
    if (CONFIG_ZTEST)
        list (APPEND SRC_FILES heartbeat_test.c)
    endif ()
    orb_library(
        ${SRC_FILES}
    )
endif ()
