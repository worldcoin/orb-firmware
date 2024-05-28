if (CONFIG_ORB_LIB_ERRORS)
    if (CONFIG_ORB_LIB_ERRORS_TESTS)
        zephyr_compile_definitions(WATCHDOG_RELOAD_MS=5000)
    endif ()

    orb_library(
        app_assert.c
        orb_fatal.c
        errors_tests.c
    )
endif ()
