/******************************************************************************
 * @file polarizer_wheel_datatypes.h
 * @brief Header file for polarizer wheel datatypes and runtime context
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

typedef enum {
    POLARIZER_WHEEL_POSITION_UNKNOWN,
    POLARIZER_WHEEL_POSITION_PASS_THROUGH,
    POLARIZER_WHEEL_POSITION_45_DEGREE,
    POLARIZER_WHEEL_POSITION_90_DEGREE,
} Polarizer_Wheel_Position_t;

typedef enum {
    POLARIZER_WHEEL_AUTO_HOMING_STATE_IDLE,
    POLARIZER_WHEEL_AUTO_HOMING_STATE_IN_PROGRESS,
    POLARIZER_WHEEL_AUTO_HOMING_STATE_FAILED,
    POLARIZER_WHEEL_AUTO_HOMING_STATE_SUCCESS,
} Auto_Homing_State_t;

typedef struct {
    // General Information
    struct {
        bool init_done;
        bool auto_homing_done;
    } general;

    struct {
        bool wheel_moving;
        Polarizer_Wheel_Position_t current_position;
        Polarizer_Wheel_Position_t target_position;
    } movement;

    struct {
        Auto_Homing_State_t auto_homing_state;
        bool start_notch_found;
        uint8_t notches_encountered;
        uint32_t steps_to_last_notch;
        uint32_t steps_to_last_notch_prev;
    } auto_homing;

    struct {
        atomic_t step_count_current;
        atomic_t step_count_target;
    } step_count;

    struct {
        uint8_t spi_error;
        uint8_t init_error;
        uint8_t configure_error;
    } errors;

} Polarizer_Wheel_Instance_t;
