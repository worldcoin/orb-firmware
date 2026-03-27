#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

#define MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES   (45000)
#define MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES (90000)

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES                                      \
    (MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES - 9500)
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES                                      \
    (MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES + 9500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES (0) // facing user
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES                                      \
    (MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES + 23000)
#endif
#define MIRROR_ANGLE_PHI_RANGE_MILLIDEGREES                                    \
    (MIRROR_ANGLE_PHI_MAX_MILLIDEGREES - MIRROR_ANGLE_PHI_MIN_MILLIDEGREES)

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MOTOR_THETA_ARM_LENGTH_MM 12.0
#define MOTOR_PHI_ARM_LENGTH_MM   18.71

// EV2 and later
#define MOTOR_THETA_CENTER_FROM_END_STEPS 55000
#define MOTOR_PHI_CENTER_FROM_END_STEPS   87000
#define MOTOR_THETA_FULL_RANGE_STEPS      (2 * MOTOR_THETA_CENTER_FROM_END_STEPS)
#define MOTOR_PHI_FULL_RANGE_STEPS        (2 * MOTOR_PHI_CENTER_FROM_END_STEPS)

#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES (90000 - 17500)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES (90000 + 17500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MOTOR_THETA_ARM_LENGTH_MM         18.0
#define MOTOR_PHI_ARM_LENGTH_MM           16.0

/*
 * Motor stroke definitions
 * 1 turn = 360º / 18º * 256 µ-steps
 */
// theta
#define MOTOR_THETA_CENTER_FROM_END_TURNS (15.4)
#define MOTOR_THETA_CENTER_FROM_END_STEPS                                      \
    (MOTOR_THETA_CENTER_FROM_END_TURNS * (360 / 18) * 256)
#define MOTOR_THETA_FULL_RANGE_STEPS          (MOTOR_THETA_CENTER_FROM_END_STEPS * 2)
// phi
#define MOTOR_PHI_CENTER_FROM_INNER_END_TURNS (16.325)
#define MOTOR_PHI_CENTER_FROM_FLAT_TURNS      (30.15)
#define MOTOR_PHI_CENTER_FROM_INNER_END_STEPS                                  \
    (MOTOR_PHI_CENTER_FROM_INNER_END_TURNS * (360 / 18) * 256)
#define MOTOR_PHI_CENTER_FROM_FLAT_END_STEPS                                   \
    (MOTOR_PHI_CENTER_FROM_FLAT_TURNS * (360 / 18) * 256)
#define MOTOR_PHI_OFF_THE_WALL_STEPS (2 * (360 / 18) * 256) // 2 turns
#define MOTOR_PHI_FULL_RANGE_STEPS                                             \
    ((MOTOR_PHI_CENTER_FROM_INNER_END_TURNS +                                  \
      MOTOR_PHI_CENTER_FROM_FLAT_TURNS) *                                      \
     (360 / 18) * 256)
#define MOTOR_PHI_CENTER_FROM_END_STEPS MOTOR_PHI_CENTER_FROM_INNER_END_STEPS

#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES                                    \
    (MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES - 20000)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES                                    \
    (MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES + 20000)
#endif
#define MIRROR_ANGLE_THETA_RANGE_MILLIDEGREES                                  \
    (MIRROR_ANGLE_THETA_MAX_MILLIDEGREES - MIRROR_ANGLE_THETA_MIN_MILLIDEGREES)

enum mirror_homing_state {
    AH_UNINIT,
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    AH_SHIFTED_SIDEWAYS,
    AH_UP_TO_WALL,
    AH_THETA_TO_CENTER,
    AH_THETA_HOMED,
#endif
    AH_GO_HOME,
    AH_WAIT_STANDSTILL,
    AH_SUCCESS,
};

/**
 * MOTOR_THETA_ANGLE:
 * If this motor is driven, then the mirror rotates around its horizontal axis.
 * This means if you look through the optics system you see a up/down movement
 * primarily (you also see a smaller amount of left/right movement, because the
 * movement of the mirror in one axis affects movement of the viewing angle in
 * both axes).
 *
 * MOTOR_PHI_ANGLE
 * If this motor is driven, then the mirror rotates around its vertical axis.
 * This means if you look through the optics system you see a left/right
 * movement. (If the current theta angle is different from 90° you also see a
 * small up/down movement).
 */
#if defined(CONFIG_BOARD_PEARL_MAIN)
typedef enum { MOTOR_THETA_ANGLE = 0, MOTOR_PHI_ANGLE, MOTORS_COUNT } motor_t;
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
typedef enum { MOTOR_PHI_ANGLE = 0, MOTOR_THETA_ANGLE, MOTORS_COUNT } motor_t;
#else
#error "unsupported board definition"
#endif

// Motors configuration
#define MOTOR_FS_VMAX 800000

typedef struct {
    int32_t steps_at_center_position; //!< Xtarget (in microsteps) to center
    uint32_t full_stroke_steps;       //!< in microsteps
    uint8_t velocity_mode_current;
#ifdef CONFIG_BOARD_PEARL_MAIN
    enum mirror_homing_state auto_homing_state;
#endif
    uint32_t motor_state;
    uint32_t angle_millidegrees;
} motors_refs_t;

extern const int32_t mirror_center_angles[MOTORS_COUNT];

int32_t
calculate_millidegrees_from_center_position(
    int32_t microsteps_from_center_position, const double motors_arm_length_mm);

int32_t
calculate_microsteps_from_center_position(
    int32_t angle_from_center_millidegrees, const double motors_arm_length_mm);

/**
 * Get the Zephyr stepper device for a motor
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @return pointer to the stepper device
 */
const struct device *
mirror_get_stepper_dev(motor_t motor);
