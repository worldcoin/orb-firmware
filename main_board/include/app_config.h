#pragma once

///////////////////////////////////////
/// Threads config                  ///
///////////////////////////////////////

// work queue CONFIG_SYSTEM_WORKQUEUE_PRIORITY = -1

/// See other threads configured with KConfig: CONFIG_ORB_LIB_THREAD_*

// Power management thread
#define THREAD_PRIORITY_POWER_MANAGEMENT   3
#define THREAD_STACK_SIZE_POWER_MANAGEMENT 2048

// Runner / default message processing
// ⚠️ flash page buffer allocated on stack for CRC32 calculation
#define THREAD_PRIORITY_RUNNER   6
#define THREAD_STACK_SIZE_RUNNER 3500

// Front unit RGB LEDs
#define THREAD_PRIORITY_FRONT_UNIT_RGB_LEDS 5
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#define THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS 1024
#else
#define THREAD_STACK_SIZE_FRONT_UNIT_RGB_LEDS 512
#endif

// Operator RGB LEDs
#define THREAD_PRIORITY_OPERATOR_RGB_LEDS 8
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
#define THREAD_STACK_SIZE_OPERATOR_RGB_LEDS 1024
#else
#define THREAD_STACK_SIZE_OPERATOR_RGB_LEDS 512
#endif

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
// Cone RGB LEDs
#define THREAD_PRIORITY_CONE_RGB_LEDS   7
#define THREAD_STACK_SIZE_CONE_RGB_LEDS 1024

// Cone button
#define THREAD_PRIORITY_CONE_BUTTON   8
#define THREAD_STACK_SIZE_CONE_BUTTON 592

// white LEDs
#define THREAD_PRIORITY_WHITE_LEDS   9
#define THREAD_STACK_SIZE_WHITE_LEDS 512
#endif

// Liquid Lens
#define THREAD_PRIORITY_LIQUID_LENS   7
#define THREAD_STACK_SIZE_LIQUID_LENS 512

// Temperature
#define THREAD_PRIORITY_TEMPERATURE   8
#define THREAD_STACK_SIZE_TEMPERATURE 2048

// Fan Tachometer
#define THREAD_PRIORITY_FAN_TACH   8
#define THREAD_STACK_SIZE_FAN_TACH 2024

// 1D ToF
#define THREAD_PRIORITY_1DTOF   9
#define THREAD_STACK_SIZE_1DTOF 2048

// ALS
#define THREAD_PRIORITY_ALS   11
#define THREAD_STACK_SIZE_ALS 2048

#define THREAD_PRIORITY_MIRROR_INIT   12
#define THREAD_STACK_SIZE_MIRROR_INIT 2048 // * 2 threads (pearl)

// Polarizer Wheel Homing
#define THREAD_PRIORITY_POLARIZER_WHEEL_HOME   8
#define THREAD_STACK_SIZE_POLARIZER_WHEEL_HOME 1024

#define THREAD_PRIORITY_BATTERY   8
#define THREAD_STACK_SIZE_BATTERY 2048

// Publish stored messages
#define THREAD_PRIORITY_PUB_STORED   9
#define THREAD_STACK_SIZE_PUB_STORED (3000)

#define THREAD_PRIORITY_GNSS   12
#define THREAD_STACK_SIZE_GNSS 2048

#define THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC1   10
#define THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC1 512

#define THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC4   10
#define THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC4 512

#define THREAD_PRIORITY_VOLTAGE_MEASUREMENT_ADC5   10
#define THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_ADC5 512

#define THREAD_PRIORITY_VOLTAGE_MEASUREMENT_DEBUG   12
#define THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_DEBUG 1024

#define THREAD_PRIORITY_VOLTAGE_MEASUREMENT_PUBLISH   11
#define THREAD_STACK_SIZE_VOLTAGE_MEASUREMENT_PUBLISH 2048

// Testing threads
// - CAN
// - DFU
#define THREAD_PRIORITY_TESTS 9

// IR camera system test
#define THREAD_PRIORITY_IR_CAMERA_SYSTEM_TEST   9
#define THREAD_STACK_SIZE_IR_CAMERA_SYSTEM_TEST 512

// main thread priority                 10
// logging thread priority              14

#define SYS_INIT_UI_LEDS_PRIORITY               62
#define SYS_INIT_I2C1_INIT_PRIORITY             55
#define SYS_INIT_POWER_SUPPLY_INIT_PRIORITY     54
#define SYS_INIT_WAIT_FOR_BUTTON_PRESS_PRIORITY 53
#define SYS_INIT_GPIO_CONFIG_PRIORITY           52

///////////////////////////////////////
/// CAN bus config                  ///
///////////////////////////////////////

#ifndef CONFIG_CAN_ADDRESS_MCU
#define CONFIG_CAN_ADDRESS_MCU 0x01
#endif

#define CAMERA_SWEEP_INTERRUPT_PRIO     10
#define LED_940NM_GLOBAL_INTERRUPT_PRIO 11
#define LED_850NM_GLOBAL_INTERRUPT_PRIO 11
