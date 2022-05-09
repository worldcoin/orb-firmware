zephyr_sources_ifdef(CONFIG_ORB_LIB_ERRORS
        app_assert.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_LIB_ERRORS include)
