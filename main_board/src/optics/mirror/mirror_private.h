#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES (45000 - 9500)
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES (45000 + 9500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MIRROR_ANGLE_PHI_MIN_MILLIDEGREES (45000 - 13000)
#define MIRROR_ANGLE_PHI_MAX_MILLIDEGREES (45000 + 13000)
#endif
#define MIRROR_ANGLE_PHI_RANGE_MILLIDEGREES                                    \
    (MIRROR_ANGLE_PHI_MAX_MILLIDEGREES - MIRROR_ANGLE_PHI_MIN_MILLIDEGREES)

#if defined(CONFIG_BOARD_PEARL_MAIN)
#define MOTOR_THETA_ARM_LENGTH_MM 12.0f
#define MOTOR_PHI_ARM_LENGTH_MM   18.71f

#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES (90000 - 17500)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES (90000 + 17500)
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
#define MOTOR_THETA_ARM_LENGTH_MM 12.0f
#define MOTOR_PHI_ARM_LENGTH_MM   12.0f

#define MIRROR_ANGLE_THETA_MIN_MILLIDEGREES (90000 - 20000)
#define MIRROR_ANGLE_THETA_MAX_MILLIDEGREES (90000 + 20000)
#endif
#define MIRROR_ANGLE_THETA_RANGE_MILLIDEGREES                                  \
    (MIRROR_ANGLE_THETA_MAX_MILLIDEGREES - MIRROR_ANGLE_THETA_MIN_MILLIDEGREES)

#define MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES                                 \
    ((MIRROR_ANGLE_THETA_MIN_MILLIDEGREES +                                    \
      MIRROR_ANGLE_THETA_MAX_MILLIDEGREES) /                                   \
     2)
#define MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES                                   \
    ((MIRROR_ANGLE_PHI_MIN_MILLIDEGREES + MIRROR_ANGLE_PHI_MAX_MILLIDEGREES) / \
     2)

enum mirror_homing_state {
    AH_UNINIT,
    AH_INITIAL_SHIFT,
    AH_LOOKING_FIRST_END,
    AH_WAIT_STANDSTILL,
    AH_GO_OTHER_END,
    AH_SUCCESS,
    AH_FAIL,
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
#endif

// Motors configuration
#define MOTOR_INIT_VMAX 100000
#define MOTOR_INIT_AMAX (MOTOR_INIT_VMAX / 20)
#define MOTOR_FS_VMAX   800000
#define IHOLDDELAY      (1 << 16)

typedef enum tmc5041_registers_e {
    REG_IDX_RAMPMODE,
    REG_IDX_XACTUAL,
    REG_IDX_VACTUAL,
    REG_IDX_VSTART,
    REG_IDX_VMAX,
    REG_IDX_XTARGET,
    REG_IDX_IHOLD_IRUN,
    REG_IDX_SW_MODE,
    REG_IDX_RAMP_STAT,
    REG_IDX_COOLCONF,
    REG_IDX_DRV_STATUS,
    REG_IDX_COUNT
} tmc5041_registers_t;

typedef struct {
    int32_t steps_at_center_position;
    uint32_t full_course;
    uint8_t velocity_mode_current;
    uint8_t stall_guard_threshold;
    enum mirror_homing_state auto_homing_state;
    uint32_t motor_state;
    uint32_t angle_millidegrees;
} motors_refs_t;

extern const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][MOTORS_COUNT];
extern const uint64_t position_mode_full_speed[MOTORS_COUNT][10];
extern const uint32_t motors_full_course_maximum_steps[MOTORS_COUNT];
extern const int32_t mirror_center_angles[MOTORS_COUNT];

int32_t
calculate_millidegrees_from_center_position(
    int32_t microsteps_from_center_position, const float motors_arm_length_mm);

int32_t
calculate_microsteps_from_center_position(
    int32_t angle_from_center_millidegrees, const float motors_arm_length_mm);

void
motor_controller_spi_send_commands(const uint64_t *cmds, size_t num_cmds);

int
motor_controller_spi_write(uint8_t reg, int32_t value);

uint32_t
motor_controller_spi_read(uint8_t reg);

bool
motor_spi_ready();
