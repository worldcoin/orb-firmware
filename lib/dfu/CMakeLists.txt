if (CONFIG_ORB_LIB_DFU)
    orb_library(
            dfu.c
            flash_map_extended.c
    )
endif ()
