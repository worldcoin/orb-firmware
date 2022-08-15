#ifndef ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
#define ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H

///////////////////////////////////////
/// Threads ordered by priorities   ///
///////////////////////////////////////

// work queue CONFIG_SYSTEM_WORKQUEUE_PRIORITY = -1

/// See other threads configured with KConfig: CONFIG_ORB_LIB_THREAD_*

// Power management thread
#define THREAD_PRIORITY_POWER_MANAGEMENT   3
#define THREAD_STACK_SIZE_POWER_MANAGEMENT 512

// Runner / default message processing
#define THREAD_PRIORITY_RUNNER   6
#define THREAD_STACK_SIZE_RUNNER 2048

// Front unit RGB LEDs
#define THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS   7
#define THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS 512

// Operator RGB LEDs
#define THREAD_PRIORITY_OPERATOR_RGB_LEDS   7
#define THREAD_STACK_SIZE_OPERATOR_RGB_LEDS 512

// Liquid Lens
#define THREAD_PRIORITY_LIQUID_LENS   7
#define THREAD_STACK_SIZE_LIQUID_LENS 512

// Temperature
#define THREAD_PRIORITY_TEMPERATURE   8
#define THREAD_STACK_SIZE_TEMPERATURE 1024

#define THREAD_PRIORITY_GNSS   9
#define THREAD_STACK_SIZE_GNSS 1024

#define THREAD_PRIORITY_MOTORS_INIT 8

// Testing threads
// - CAN
// - DFU
#define THREAD_PRIORITY_TESTS 9

// IR camera system test
#define THREAD_PRIORITY_IR_CAMERA_SYSTEM_TEST   9
#define THREAD_STACK_SIZE_IR_CAMERA_SYSTEM_TEST 512

// main thread priority                 10
// logging thread priority              14

#define SYS_INIT_POWER_SUPPLY_PHASE2_PRIORITY   55
#define SYS_INIT_FAN_INIT_PRIORITY              54
#define SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY 53
#define SYS_INIT_UI_LEDS_PRIORITY               52
#define SYS_INIT_POWER_SUPPLY_PHASE1_PRIORITY   51

///////////////////////////////////////
/// CAN bus config                  ///
///////////////////////////////////////

#ifndef CONFIG_CAN_ADDRESS_MCU
#define CONFIG_CAN_ADDRESS_MCU 0x01
#endif

#endif // ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
