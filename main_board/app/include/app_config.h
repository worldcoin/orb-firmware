//
// Created by Cyril on 05/10/2021.
//

#ifndef ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
#define ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H

// Threads priorities

// work queue                           CONFIG_SYSTEM_WORKQUEUE_PRIORITY = -1
#define THREAD_PRIORITY_CAN_RX         5
#define THREAD_PRIORITY_PROCESS_TX_MSG 7
#define THREAD_PRIORITY_MOTORS_INIT    8
#define THREAD_PRIORITY_TESTS          9
// main thread priority                 10 // set as
// logging thread priority              14

// Front unit RGB LEDs
#define THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS   4
#define THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS 256

// IR camera system test
#define THREAD_PRIORITY_IR_CAMERA_SYSTEM_TEST   4
#define THREAD_STACK_SIZE_IR_CAMERA_SYSTEM_TEST 512

// Temperature
#define THREAD_PRIORITY_TEMPERATURE   7
#define THREAD_STACK_SIZE_TEMPERATURE 512

#define ESSENTIAL_POWER_SUPPLIES_INIT_PRIORITY 55

#ifndef CONFIG_CAN_ADDRESS_MCU
#define CONFIG_CAN_ADDRESS_MCU 0x01
#endif

#ifndef CONFIG_CAN_ADDRESS_JETSON
#define CONFIG_CAN_ADDRESS_JETSON 0x80
#endif

#endif // ORB_FIRMWARE_APP_MAIN_INCLUDE_APP_CONFIG_H
