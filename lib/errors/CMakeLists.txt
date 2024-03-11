if (CONFIG_ORB_LIB_ERRORS)
    if (CONFIG_ORB_LIB_ERRORS_TESTS)
        set(OPTIONAL_SOURCE_FILES errors_tests.c)
        zephyr_compile_definitions(WATCHDOG_RELOAD_MS=5000)
    endif ()

    orb_library(
        app_assert.c
        orb_fatal.c
        ${OPTIONAL_SOURCE_FILES}
    )
endif ()
