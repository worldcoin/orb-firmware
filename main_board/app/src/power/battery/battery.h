#pragma once

#include "errors.h"
#include "mcu_messaging.pb.h"

// minimum voltage needed to boot the Orb during startup (in millivolts)
#define BATTERY_MINIMUM_VOLTAGE_STARTUP_MV       13750
#define BATTERY_MINIMUM_VOLTAGE_RUNTIME_MV       12500
#define BATTERY_MINIMUM_CAPACITY_STARTUP_PERCENT 5

/**
 * Initialize main battery module
 *
 * @details This function initializes any communication to the main battery
 *      and makes sure that the battery is not critically low.
 *      It also spawns a thread that monitors the battery stats.
 *      The module also accepts that the Orb is powered by a lab power supply
 *      (for dev purposes) by reading the analog voltage on the
 *      corresponding pin.
 *
 * @retval RET_SUCCESS battery module is receiving messages from the battery
 * @retval RET_ERROR_INVALID_STATE if the CAN device or GPIO cannot be operated
 * @retval RET_ERROR_INTERNAL if the CAN device is ready but the CAN device
 *      cannot be configured to receive messages from the battery
 */
ret_code_t
battery_init(void);
