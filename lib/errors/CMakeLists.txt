zephyr_sources_ifdef(CONFIG_ORB_APP_ASSERT
        app_assert.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_APP_ASSERT include)
