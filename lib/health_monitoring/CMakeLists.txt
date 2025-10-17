if (CONFIG_ORB_LIB_HEALTH_MONITORING)
    set (SRC_FILES heartbeat.c orb_state.c mcu_ping.c)
    if (CONFIG_ZTEST)
        list (APPEND SRC_FILES heartbeat_test.c)
    endif ()
    orb_library(
        ${SRC_FILES}
    )
    zephyr_iterable_section(NAME orb_state_dynamic GROUP DATA_REGION ${XIP_ALIGN_WITH_INPUT} SUBALIGN ${CONFIG_LINKER_ITERABLE_SUBALIGN})
    zephyr_iterable_section(NAME orb_state_const KVMA RAM_REGION GROUP RODATA_REGION SUBALIGN ${CONFIG_LINKER_ITERABLE_SUBALIGN})
endif ()
