set(LIBS
    CONFIG_ORB_LIB_ERRORS
    CONFIG_ORB_LIB_DFU
    CONFIG_ORB_LIB_CAN_MESSAGING
    CONFIG_ORB_LIB_UART_MESSAGING
    CONFIG_ORB_LIB_HEALTH_MONITORING
    CONFIG_ORB_LIB_LOG_BACKEND_CAN
    CONFIG_ORB_I2C_SCAN
    CONFIG_ORB_LIB_STORAGE
    CONFIG_ORB_LIB_WATCHDOG
)

set(PREFIX "CONFIG_ORB_LIB_")
string(LENGTH ${PREFIX} PREFIX_LENGTH)
set(at_least_one FALSE)

foreach (lib ${LIBS})
    if (${${lib}})
        string(SUBSTRING ${lib} ${PREFIX_LENGTH} -1 directory)
        string(TOLOWER ${directory} directory)
        add_subdirectory(${directory})
        set(at_least_one TRUE)
    endif ()
endforeach ()

if (${at_least_one})
    zephyr_include_directories(include)
endif()
