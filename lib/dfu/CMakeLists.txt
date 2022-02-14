zephyr_sources_ifdef(CONFIG_ORB_LIB_DFU
        dfu.c
        flash_map_extended.c
        )

zephyr_include_directories_ifdef(CONFIG_ORB_LIB_DFU include)
