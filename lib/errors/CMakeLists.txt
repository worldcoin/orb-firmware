zephyr_interface_library_named(orb-lib-errors)

zephyr_library_sources_ifdef(CONFIG_ORB_APP_ASSERT
        app_assert.c
        )

zephyr_include_directories(include)
