if (CONFIG_ORB_LIB_STORAGE)

    list(APPEND SRC_FILES storage.c)

    if (CONFIG_BBRAM)
        list(APPEND SRC_FILES backup_regs.c)
    endif ()

    if (CONFIG_ORB_LIB_STORAGE_TESTS)
        list(APPEND SRC_FILES storage_tests.c)
    endif ()

    orb_library(
        ${SRC_FILES}
    )

endif ()
