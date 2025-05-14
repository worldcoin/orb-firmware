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

enum polarizer_wheel_angle_e {
    POLARIZER_WHEEL_POSITION_PASS_THROUGH_ANGLE = 0,
    POLARIZER_WHEEL_VERTICALLY_POLARIZED_ANGLE = 1200,
    POLARIZER_WHEEL_HORIZONTALLY_POLARIZED_ANGLE = 2400,
};

enum polarizer_wheel_direction_e {
    POLARIZER_WHEEL_DIRECTION_BACKWARD = -1,
    POLARIZER_WHEEL_DIRECTION_FORWARD = 1,
};

typedef struct {
    // polarizer wheel status
    orb_mcu_HardwareDiagnostic_Status status;

    struct {
        uint8_t notch_count;
        bool success;
    } homing;

    struct {
        // microsteps into range: [0; POLARIZER_WHEEL_MICROSTEPS_360_DEGREES]
        atomic_t current;
        atomic_t target;
        enum polarizer_wheel_direction_e direction;
    } step_count;
} polarizer_wheel_instance_t;
