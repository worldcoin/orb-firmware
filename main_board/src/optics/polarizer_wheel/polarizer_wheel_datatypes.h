#pragma once

/******************************************************************************
 * @file polarizer_wheel_datatypes.h
 * @brief Header file for polarizer wheel datatypes and runtime context
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include <common.pb.h>
#include <zephyr/sys/atomic.h>

typedef enum {
    POLARIZER_WHEEL_POSITION_UNKNOWN = -1,
    POLARIZER_WHEEL_POSITION_PASS_THROUGH = 0,
    POLARIZER_WHEEL_POSITION_0_DEGREE = 2048,
    POLARIZER_WHEEL_POSITION_90_DEGREE = 4096,
} polarizer_wheel_position_t;

typedef struct {
    // polarizer wheel status
    orb_mcu_HardwareDiagnostic_Status status;

    struct {
        uint8_t notch_count;
        bool success;
    } homing;

    struct {
        atomic_t current;
        atomic_t target;
    } step_count;
} polarizer_wheel_instance_t;
