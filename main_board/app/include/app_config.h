//
// Created by Cyril on 05/10/2021.
//

#ifndef ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
#define ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H

///////////////////////////////////////
/// Threads ordered by priorities   ///
///////////////////////////////////////

// work queue CONFIG_SYSTEM_WORKQUEUE_PRIORITY = -1

/// See other threads configured with KConfig: CONFIG_ORB_LIB_THREAD_*

// Power management thread
#define THREAD_PRIORITY_POWER_MANAGEMENT   3
#define THREAD_STACK_SIZE_POWER_MANAGEMENT 256

// Front unit RGB LEDs
#define THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS   6
#define THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS 512

// Temperature
#define THREAD_PRIORITY_TEMPERATURE   8
#define THREAD_STACK_SIZE_TEMPERATURE 1024

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

#define ESSENTIAL_POWER_SUPPLIES_INIT_PRIORITY 55
#define FAN_INIT_PRIORITY                      54

///////////////////////////////////////
/// CAN bus config                  ///
///////////////////////////////////////

#ifndef CONFIG_CAN_ADDRESS_MCU
#define CONFIG_CAN_ADDRESS_MCU 0x01
#endif

#endif // ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
