zephyr_sources_ifdef(CONFIG_ORB_LIB_HEALTH_MONITORING
        heartbeat.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_LIB_HEALTH_MONITORING include)
