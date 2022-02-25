zephyr_sources_ifdef(CONFIG_ORB_LIB_LOG_BACKEND_CAN
        log_backend_can.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_LIB_LOG_BACKEND_CAN include)
