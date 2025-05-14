#pragma once

/******************************************************************************
 * @file polarizer_wheel_datatypes.h
 * @brief Header file for polarizer wheel datatypes and runtime context
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include <zephyr/sys/atomic.h>

typedef enum {
    POLARIZER_WHEEL_POSITION_UNKNOWN = -1,
    POLARIZER_WHEEL_POSITION_PASS_THROUGH = 0,
    POLARIZER_WHEEL_POSITION_0_DEGREE = 2048,
    POLARIZER_WHEEL_POSITION_90_DEGREE = 4096,
} polarizer_wheel_position_t;

typedef enum {
    POLARIZER_WHEEL_AUTO_HOMING_STATE_IN_PROGRESS,
    POLARIZER_WHEEL_AUTO_HOMING_STATE_FAILED,
    POLARIZER_WHEEL_AUTO_HOMING_STATE_SUCCESS,
} polarizer_homing_state_t;

typedef struct {
    // General Information
    struct {
        bool init_done;
        bool auto_homing_done;
    } general;

    struct {
        polarizer_homing_state_t state;
        uint8_t notch_count;
    } homing;

    struct {
        atomic_t current;
        atomic_t target;
    } step_count;

    struct {
        uint8_t spi_error;
        uint8_t init_error;
        uint8_t configure_error;
    } errors;

} polarizer_wheel_instance_t;
