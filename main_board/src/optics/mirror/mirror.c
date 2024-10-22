#include "mirror.h"
#include "mcu.pb.h"
#include "orb_logs.h"
#include "pubsub/pubsub.h"
#include "system/version/version.h"
#include <app_assert.h>
#include <app_config.h>
#include <math.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

// _ANSI_SOURCE is used with newlib to make sure newlib doesn't provide
// primitives conflicting with Zephyr's POSIX definitions which remove
// definition of M_PI, so let's redefine it
#if defined(CONFIG_NEWLIB_LIBC) && defined(_ANSI_SOURCE)
// taken from math.h
#define M_PI 3.14159265358979323846f
#endif

LOG_MODULE_REGISTER(mirror, CONFIG_MIRROR_LOG_LEVEL);

K_THREAD_STACK_DEFINE(stack_area_mirror_work_queue, 2048);
K_THREAD_STACK_DEFINE(stack_area_motor_phi_init, 2048);
K_THREAD_STACK_DEFINE(stack_area_motor_theta_init, 2048);

static struct k_thread thread_data_motor_phi;
static struct k_thread thread_data_motor_theta;
static struct k_sem homing_in_progress_sem[MOTORS_COUNT];

static struct k_work_q mirror_work_queue;

static struct async_mirror_command {
    struct k_work work;
    int32_t angle_millidegrees;
} theta_angle_set_work_item, phi_angle_set_work_item;

// To get motor driver status, we need to poll its register (interrupt pins not
// connected)
// Below are timing definitions
#define AUTOHOMING_POLL_DELAY_MS 30
#define AUTOHOMING_TIMEOUT_MS    7000
#define AUTOHOMING_TIMEOUT_LOOP_COUNT                                          \
    (AUTOHOMING_TIMEOUT_MS / AUTOHOMING_POLL_DELAY_MS)

// before starting auto-homing, we drive to motor in the opposite direction
// of the first end reached with stall detection, to make sure the motor is not
// close from the first end
#define AUTOHOMING_AWAY_FROM_BARRIER_STEPS ((int32_t)20000)

// SPI w/ TMC5041
#define SPI_DEVICE          DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ  0
#define WRITE (1 << 7)

static struct spi_config spi_cfg = {.frequency = 1000000,
                                    .operation = SPI_WORD_SET(8) |
                                                 SPI_OP_MODE_MASTER |
                                                 SPI_MODE_CPOL | SPI_MODE_CPHA,
                                    .cs = SPI_CS_CONTROL_INIT(SPI_DEVICE, 2)};
static const struct device *spi_bus_controller =
    DEVICE_DT_GET(SPI_CONTROLLER_NODE);

static struct spi_buf rx;
static struct spi_buf_set rx_bufs = {.buffers = &rx, .count = 1};

static struct spi_buf tx;
static struct spi_buf_set tx_bufs = {.buffers = &tx, .count = 1};

#define TMC5041_IC_VERSION 0x10

#define TMC5041_REG_GCONF 0x00
#define REG_INPUT         0x04

// Motors configuration
#define MOTOR_INIT_VMAX 100000
#define MOTOR_INIT_AMAX (MOTOR_INIT_VMAX / 20)
#define MOTOR_FS_VMAX   800000
#define IHOLDDELAY      (1 << 16)

// initial values [IRUN, SGT]
const uint8_t motor_irun_sgt[MOTORS_COUNT][2] = {
    [MOTOR_THETA_ANGLE] = {0x13, 6},
    [MOTOR_PHI_ANGLE] = {0x13, 6},
};

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

const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][MOTORS_COUNT] = {
    {0x20, 0x40}, // RAMPMODE
    {0x21, 0x41}, // XACTUAL
    {0x22, 0x42}, // VACTUAL
    {0x23, 0x43}, // VSTART
    {0x27, 0x47}, // VMAX
    {0x2D, 0x4D}, // XTARGET
    {0x30, 0x50}, // IHOLD_IRUN
    {0x34, 0x54}, // SW_MODE
    {0x35, 0x55}, // RAMP_STAT
    {0x6D, 0x7D}, // COOLCONF
    {0x6F, 0x7F}, // DRV_STATUS
};

// a bit more than mechanical range
static const uint32_t motors_full_course_maximum_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = (500 * 256), [MOTOR_PHI_ANGLE] = (700 * 256)};
static const int32_t mirror_center_angles[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES,
    [MOTOR_PHI_ANGLE] = MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES};

#if defined(CONFIG_BOARD_PEARL_MAIN)
// EV2 and later
#define MOTOR_THETA_CENTER_FROM_END_STEPS 55000
#define MOTOR_PHI_CENTER_FROM_END_STEPS   87000
#define MOTOR_THETA_FULL_RANGE_STEPS      (2 * MOTOR_THETA_CENTER_FROM_END_STEPS)
#define MOTOR_PHI_FULL_RANGE_STEPS        (2 * MOTOR_PHI_CENTER_FROM_END_STEPS)

#define MOTOR_THETA_ARM_LENGTH_MM 12.0f
#define MOTOR_PHI_ARM_LENGTH_MM   18.71f
#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
// values measured on first PoC Diamond Orb
#define MOTOR_THETA_CENTER_FROM_END_STEPS 63000
#define MOTOR_PHI_CENTER_FROM_END_STEPS   47600
#define MOTOR_THETA_FULL_RANGE_STEPS      140000
#define MOTOR_PHI_FULL_RANGE_STEPS        102400

#define MOTOR_THETA_ARM_LENGTH_MM 12.0f
#define MOTOR_PHI_ARM_LENGTH_MM   12.0f
#endif

static const uint32_t motors_center_from_end_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_CENTER_FROM_END_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_CENTER_FROM_END_STEPS};
static const uint32_t motors_full_range_steps[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_FULL_RANGE_STEPS,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_FULL_RANGE_STEPS};
const float motors_arm_length_mm[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MOTOR_THETA_ARM_LENGTH_MM,
    [MOTOR_PHI_ANGLE] = MOTOR_PHI_ARM_LENGTH_MM};

const uint32_t microsteps_per_mm = 12800; // 1mm / 0.4mm (pitch) * (360° / 18°
                                          // (per step)) * 256 micro-steps

enum mirror_autohoming_state {
    AH_UNINIT,
    AH_INITIAL_SHIFT,
    AH_LOOKING_FIRST_END,
    AH_WAIT_STANDSTILL,
    AH_GO_OTHER_END,
    AH_SUCCESS,
    AH_FAIL,
};

typedef struct {
    int32_t steps_at_center_position;
    uint32_t full_course;
    uint8_t velocity_mode_current;
    uint8_t stall_guard_threshold;
    enum mirror_autohoming_state auto_homing_state;
    uint32_t motor_state;
    uint32_t angle_millidegrees;
} motors_refs_t;

static motors_refs_t motors_refs[MOTORS_COUNT] = {0};

#if AUTO_HOMING_ENABLED

// minimum number of microsteps for 40º range
static const uint32_t motors_full_course_minimum_steps[MOTORS_COUNT] = {
    (300 * 256), (325 * 256)};

/// One direction with stall guard detection
/// Velocity mode
const uint64_t motor_init_for_velocity_mode[MOTORS_COUNT][8] = {
    [0] =
        {
            0xEC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                          // (spreadCycle)
            0xAC00000010, // TZEROWAIT
            0x90000401C8, // PWMCONF
            0xB200061A80,
            0xB100000000 +
                (MOTOR_INIT_VMAX * 9 / 10), // VCOOLTHRS: StallGuard enabled
                                            // when motor reaches that velocity
            0xA600000000 + MOTOR_INIT_AMAX, // AMAX = acceleration and
                                            // deceleration in velocity mode
            0xA700000000 + MOTOR_INIT_VMAX, // VMAX target velocity
            0xB400000000 /* | MOTOR_DRV_SW_MODE_SG_STOP */, // SW_MODE sg_stop
                                                            // disabled, motors
                                                            // are
            // stopped using software
            // command
        },

    [1] = {
        0xFC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xCC00000010, // TZEROWAIT
        0x98000401C8, // PWMCONF
        0xD200061A80,
        0xD100000000 +
            (MOTOR_INIT_VMAX * 9 / 10), // VCOOLTHRS: StallGuard enabled when
                                        // motor reaches that velocity
        0xC600000000 + MOTOR_INIT_AMAX, // AMAX = acceleration and deceleration
                                        // in velocity mode
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX target velocity
        0xD400000000 /* | MOTOR_DRV_SW_MODE_SG_STOP */, // SW_MODE sg_stop
                                                        // disabled, motors are
                                                        // stopped using
                                                        // software command
    }}; // RAMPMODE velocity mode to +VMAX using AMAX

const uint64_t position_mode_initial_phase[MOTORS_COUNT][10] = {
    [0] =
        {
            0xEC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                          // (spreadCycle)
            0xB000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
            0xA4000003E8, // A1 = 1000 first acceleration
            0xA50000C350, // V1 = 50 000 Acceleration threshold, velocity V1
            0xA6000001F4, // AMAX = 500 Acceleration above V1
            0xA700000000 + MOTOR_INIT_VMAX, // VMAX
            0xA8000002BC,                   // DMAX Deceleration above V1
            0xAA00000578,                   // D1 Deceleration below V1
            0xAB0000000A,                   // VSTOP stop velocity
            0xA000000000,                   // RAMPMODE = 0 position move
        },
    [1] = {
        0xFC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xD000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
        0xC4000003E8, // A1 = 1000 first acceleration
        0xC50000C350, // V1 = 50 000 Acceleration threshold, velocity V1
        0xC6000001F4, // AMAX = 500 Acceleration above V1
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX = 200 000
        0xC8000002BC,                   // DMAX = 700 Deceleration above V1
        0xCA00000578,                   // D1 = 1400 Deceleration below V1
        0xCB0000000A,                   // VSTOP = 10 stop velocity
        0xC000000000,                   // RAMPMODE = 0 position move
                                        // Ready to move
    }};

#endif

const uint64_t position_mode_full_speed[MOTORS_COUNT][10] = {
    [0] =
        {
            0xEC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                          // (spreadCycle)
            0xB000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
            0xA400008000, // A1 first acceleration
            0xA500000000 +
                MOTOR_FS_VMAX * 3 / 4, // V1 Acceleration threshold, velocity V1
            0xA600001000,              // Acceleration above V1
            0xA700000000 + MOTOR_FS_VMAX, // VMAX
            0xA800001000,                 // DMAX Deceleration above V1
            0xAA00008000,                 // D1 Deceleration below V1
            0xAB00000010,                 // VSTOP stop velocity
            0xA000000000,                 // RAMPMODE = 0 position move
        },
    [1] = {
        0xFC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xD000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
        0xC400008000, // A1 first acceleration
        0xC500000000 +
            MOTOR_FS_VMAX * 3 / 4,    // V1 Acceleration threshold, velocity V1
        0xC600001000,                 // Acceleration above V1
        0xC700000000 + MOTOR_FS_VMAX, // VMAX
        0xC800001000,                 // DMAX Deceleration above V1
        0xCA00008000,                 // D1 Deceleration below V1
        0xCB00000010,                 // VSTOP stop velocity
        0xC000000000,                 // RAMPMODE = 0 position move
    }};

static int32_t
calculate_millidegrees_from_center_position(
    int32_t microsteps_from_center_position, motor_t mirror)
{
    float stepper_position_from_center_millimeters =
        (float)microsteps_from_center_position / (float)microsteps_per_mm;
    int32_t angle_from_center_millidegrees =
        asinf(stepper_position_from_center_millimeters /
              motors_arm_length_mm[mirror]) *
        180000.0f / M_PI;
    return angle_from_center_millidegrees;
}

static int32_t
calculate_microsteps_from_center_position(
    int32_t angle_from_center_millidegrees, motor_t mirror)
{
    float stepper_position_from_center_millimeters =
        sinf((float)angle_from_center_millidegrees * M_PI / 180000.0f) *
        motors_arm_length_mm[mirror];
    int32_t stepper_position_from_center_microsteps = (int32_t)roundf(
        stepper_position_from_center_millimeters * (float)microsteps_per_mm);
    return stepper_position_from_center_microsteps;
}

static void
motor_controller_spi_send_commands(const struct device *spi_bus_controller,
                                   const uint64_t *cmds, size_t num_cmds)
{
    uint64_t cmd;
    uint8_t tx_buffer[5];
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    for (size_t i = 0; i < num_cmds; ++i) {
        cmd = sys_cpu_to_be64(cmds[i] << 24);
        memcpy(tx_buffer, &cmd, 5);
        spi_write(spi_bus_controller, &spi_cfg, &tx_bufs);
    }
}

static int
motor_controller_spi_write(const struct device *spi_bus_controller, uint8_t reg,
                           int32_t value)
{
    uint8_t tx_buffer[5] = {0};
    uint8_t rx_buffer[5] = {0};

    // make sure there is the write flag
    reg |= WRITE;
    tx_buffer[0] = reg;

    tx_buffer[1] = (value >> 24) & 0xFF;
    tx_buffer[2] = (value >> 16) & 0xFF;
    tx_buffer[3] = (value >> 8) & 0xFF;
    tx_buffer[4] = (value >> 0) & 0xFF;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    int ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
    ASSERT_HARD(ret);

    return RET_SUCCESS;
}

static uint32_t
motor_controller_spi_read(const struct device *spi_bus_controller, uint8_t reg)
{
    uint8_t tx_buffer[5] = {0};
    uint8_t rx_buffer[5] = {0};
    int ret;

    // make sure there is the read flag (msb is 0)
    reg &= ~WRITE;
    tx_buffer[0] = reg;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    // reading happens in two SPI operations:
    //  - first, send the register address, returned data is
    //    the one from previous read operation.
    //  - second, read the actual data

    // first, send reg address
    ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
    ASSERT_HARD(ret);

    memset(rx_buffer, 0, sizeof(rx_buffer));

    // second, read data
    ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
    ASSERT_HARD(ret);

    uint32_t read_value = (rx_buffer[1] << 24 | rx_buffer[2] << 16 |
                           rx_buffer[3] << 8 | rx_buffer[4] << 0);

    return read_value;
}

static void
mirror_set_stepper_position(int32_t position_steps, motor_t mirror)
{
    motor_controller_spi_write(spi_bus_controller,
                               TMC5041_REGISTERS[REG_IDX_XTARGET][mirror],
                               position_steps);
}

/**
 * Set relative angle from the center position
 * @param angle_from_center_millidegrees
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @return
 */
static ret_code_t
mirror_set_angle_from_center(int32_t angle_from_center_millidegrees,
                             motor_t motor)
{
    if (motor >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (motors_refs[motor].motor_state) {
        return motors_refs[motor].motor_state;
    }

    int32_t stepper_position_from_center_microsteps =
        calculate_microsteps_from_center_position(
            angle_from_center_millidegrees, motor);

    if (motor == MOTOR_PHI_ANGLE) {
        stepper_position_from_center_microsteps =
            -stepper_position_from_center_microsteps;
    }

    int32_t stepper_position_absolute_microsteps =
        motors_refs[motor].steps_at_center_position +
        stepper_position_from_center_microsteps;

    motors_refs[motor].angle_millidegrees =
        angle_from_center_millidegrees + mirror_center_angles[motor];

    LOG_INF("Setting mirror %u to %d milli-degrees (%d microsteps)", motor,
            motors_refs[motor].angle_millidegrees,
            stepper_position_absolute_microsteps);

    mirror_set_stepper_position(stepper_position_absolute_microsteps, motor);

    LOG_INF("new mirror pos from center: %d °, %d microsteps",
            angle_from_center_millidegrees,
            stepper_position_from_center_microsteps);

    return RET_SUCCESS;
}

static ret_code_t
mirror_check_angle(uint32_t angle_millidegrees, motor_t motor)
{
    switch (motor) {
    case MOTOR_THETA_ANGLE:
        if (angle_millidegrees > MIRROR_ANGLE_THETA_MAX_MILLIDEGREES ||
            angle_millidegrees < MIRROR_ANGLE_THETA_MIN_MILLIDEGREES) {
            LOG_ERR("Mirror theta angle of %d out of range [%d;%d]",
                    angle_millidegrees, MIRROR_ANGLE_THETA_MIN_MILLIDEGREES,
                    MIRROR_ANGLE_THETA_MAX_MILLIDEGREES);
            return RET_ERROR_INVALID_PARAM;
        }
        return RET_SUCCESS;
    case MOTOR_PHI_ANGLE:
        if (angle_millidegrees > MIRROR_ANGLE_PHI_MAX_MILLIDEGREES ||
            angle_millidegrees < MIRROR_ANGLE_PHI_MIN_MILLIDEGREES) {
            LOG_ERR("Mirror phi angle of %u out of range [%u;%u]",
                    angle_millidegrees, MIRROR_ANGLE_PHI_MIN_MILLIDEGREES,
                    MIRROR_ANGLE_PHI_MAX_MILLIDEGREES);
            return RET_ERROR_INVALID_PARAM;
        }
        return RET_SUCCESS;
    default:
        return RET_ERROR_INVALID_PARAM;
        break;
    }
}

/**
 * Set relative angle from the current position
 * @param angle_millidegrees from current position
 * @param motor MOTOR_THETA_ANGLE or MOTOR_PHI_ANGLE
 * @retval RET_SUCCESS on success
 * @retval RET_ERROR_FORBIDDEN if the mirror cannot be moved to the requested
 * position given its current position
 */
static ret_code_t
mirror_set_angle_relative(int32_t angle_millidegrees, motor_t motor)
{
    int32_t stepper_position_absolute_microsteps =
        (int32_t)motor_controller_spi_read(
            spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);

    int32_t stepper_position_from_center_microsteps =
        stepper_position_absolute_microsteps -
        motors_refs[motor].steps_at_center_position;

    if (motor == MOTOR_PHI_ANGLE) {
        stepper_position_from_center_microsteps =
            -stepper_position_from_center_microsteps;
    }

    int32_t angle_from_center_millidegrees =
        calculate_millidegrees_from_center_position(
            stepper_position_from_center_microsteps, motor);

    int32_t target_angle_from_center_millidegrees =
        angle_from_center_millidegrees + angle_millidegrees;
    ret_code_t ret =
        mirror_check_angle((uint32_t)(target_angle_from_center_millidegrees +
                                      mirror_center_angles[motor]),
                           motor);

    LOG_DBG(
        "Set relative angle: old_pos_microsteps = %d, from_center_microsteps = "
        "%d, angle_from_center = %d, target_angle_from_center = %d",
        stepper_position_absolute_microsteps,
        stepper_position_from_center_microsteps, angle_from_center_millidegrees,
        target_angle_from_center_millidegrees);

    if (ret != RET_SUCCESS) {
        return ret;
    }

    ret = mirror_set_angle_from_center(target_angle_from_center_millidegrees,
                                       motor);

    return ret;
}

ret_code_t
mirror_set_angle_phi_relative(int32_t angle_millidegrees)
{
    return mirror_set_angle_relative(angle_millidegrees, MOTOR_PHI_ANGLE);
}

ret_code_t
mirror_set_angle_theta_relative(int32_t angle_millidegrees)
{
    return mirror_set_angle_relative(angle_millidegrees, MOTOR_THETA_ANGLE);
}

static ret_code_t
mirror_set_angle(uint32_t angle_millidegrees, motor_t motor)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, motor);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    int32_t angle_from_center_millidegrees =
        (int32_t)angle_millidegrees - mirror_center_angles[motor];

    return mirror_set_angle_from_center(angle_from_center_millidegrees, motor);
}

ret_code_t
mirror_set_angle_phi(uint32_t angle_millidegrees)
{
    return mirror_set_angle(angle_millidegrees, MOTOR_PHI_ANGLE);
}

ret_code_t
mirror_set_angle_theta(uint32_t angle_millidegrees)
{
    return mirror_set_angle(angle_millidegrees, MOTOR_THETA_ANGLE);
}

static void
mirror_angle_theta_work_wrapper(struct k_work *item)
{
    struct async_mirror_command *cmd =
        CONTAINER_OF(item, struct async_mirror_command, work);
    mirror_set_angle_theta(cmd->angle_millidegrees);
}

static void
mirror_angle_phi_work_wrapper(struct k_work *item)
{
    struct async_mirror_command *cmd =
        CONTAINER_OF(item, struct async_mirror_command, work);
    mirror_set_angle_phi(cmd->angle_millidegrees);
}

ret_code_t
mirror_set_angle_phi_async(int32_t angle_millidegrees)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_PHI_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_busy_get(&phi_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror phi set work item is busy!");
        return RET_ERROR_BUSY;
    }

    phi_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_submit_to_queue(&mirror_work_queue, &phi_angle_set_work_item.work);

    return RET_SUCCESS;
}

ret_code_t
mirror_set_angle_theta_async(int32_t angle_millidegrees)
{
    ret_code_t ret = mirror_check_angle(angle_millidegrees, MOTOR_THETA_ANGLE);
    if (ret != RET_SUCCESS) {
        return ret;
    }

    if (k_work_busy_get(&theta_angle_set_work_item.work)) {
        LOG_ERR("async: Mirror theta set work item is busy!");
        return RET_ERROR_BUSY;
    }

    theta_angle_set_work_item.angle_millidegrees = angle_millidegrees;
    k_work_submit_to_queue(&mirror_work_queue, &theta_angle_set_work_item.work);

    return RET_SUCCESS;
}

#if AUTO_HOMING_ENABLED

#include <compilers.h>

/**
 * Increase sensitivity in three steps
 * First, increase current without modifying SGT
 * Second, decrease SGT but revert current to normal
 * Third, increase current with SGT decreased
 * @param motor
 */
static void
increase_stall_sensitivity(motor_t motor)
{
    if (motors_refs[motor].velocity_mode_current == motor_irun_sgt[motor][0] &&
        motors_refs[motor].stall_guard_threshold == motor_irun_sgt[motor][1]) {
        // default values
        // increase current first
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0] + 1;
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1];
    } else if (motors_refs[motor].velocity_mode_current ==
                   (motor_irun_sgt[motor][0] + 1) &&
               motors_refs[motor].stall_guard_threshold ==
                   motor_irun_sgt[motor][1]) {
        // increased current
        // decrease stall detection threshold
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0];
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1] - 1;
    } else if (motors_refs[motor].velocity_mode_current ==
                   motor_irun_sgt[motor][0] &&
               motors_refs[motor].stall_guard_threshold ==
                   motor_irun_sgt[motor][1] - 1) {
        // increase current once more while keeping reduced stall detection
        // threshold
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0] + 1;
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1] - 1;
    } else {
        LOG_WRN("Out of options to increase sensitivity");
    }
    LOG_DBG("Motor %u: IRUN: 0x%02x, SGT: %u", motor,
            motors_refs[motor].velocity_mode_current,
            motors_refs[motor].stall_guard_threshold);
}

/**
 * Decrease sensitivity in three steps
 * First, decrease current without modifying SGT
 * Second, increase SGT but revert current to normal
 * Third, decrease current with SGT increased
 * @param motor
 */
static void
decrease_stall_sensivity(motor_t motor)
{
    if (motors_refs[motor].velocity_mode_current == motor_irun_sgt[motor][0] &&
        motors_refs[motor].stall_guard_threshold == motor_irun_sgt[motor][1]) {
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0] - 1;
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1];
    } else if (motors_refs[motor].velocity_mode_current ==
                   (motor_irun_sgt[motor][0] - 1) &&
               motors_refs[motor].stall_guard_threshold ==
                   motor_irun_sgt[motor][1]) {
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0];
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1] + 1;
    } else if (motors_refs[motor].velocity_mode_current ==
                   motor_irun_sgt[motor][0] &&
               motors_refs[motor].stall_guard_threshold ==
                   motor_irun_sgt[motor][1] + 1) {
        motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0] - 1;
        motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1] + 1;
    } else {
        LOG_WRN("Out of options to decrease sensitivity");
    }
    LOG_DBG("Motor %u: IRUN: 0x%02x, SGT: %u", motor,
            motors_refs[motor].velocity_mode_current,
            motors_refs[motor].stall_guard_threshold);
}

static void
reset_irun_sgt(motor_t motor)
{
    motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0];
    motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1];
}

static void
to_one_direction(motor_t motor, bool positive_direction)
{
    uint8_t current = motors_refs[motor].velocity_mode_current;
    uint8_t sgt = motors_refs[motor].stall_guard_threshold;

    LOG_DBG("Current: %u, sgt: %u", current, sgt);

    if (current > 31) {
        ASSERT_SOFT(RET_ERROR_INVALID_PARAM);
        current = 31;
    }

    // COOLCONF, set SGT to offset StallGuard value
    motor_controller_spi_write(spi_bus_controller,
                               TMC5041_REGISTERS[REG_IDX_COOLCONF][motor],
                               (sgt << 16) | (1 << 24) /* enable SG filter */);

    // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
    // IHOLD = 0
    motor_controller_spi_write(spi_bus_controller,
                               TMC5041_REGISTERS[REG_IDX_IHOLD_IRUN][motor],
                               IHOLDDELAY | (current << 8));

    // start velocity mode until stall is detected ->
    motor_controller_spi_send_commands(
        spi_bus_controller, motor_init_for_velocity_mode[motor],
        ARRAY_SIZE(motor_init_for_velocity_mode[motor]));

    // let's go
    motor_controller_spi_write(spi_bus_controller,
                               TMC5041_REGISTERS[REG_IDX_RAMPMODE][motor],
                               positive_direction ? 1 : 2);
}

/**
 * @brief Perform auto-homing
 * See TMC5041 DATASHEET (Rev. 1.14 / 2020-JUN-12) page 58
 * This thread sets the motor state after auto-homing procedure
 *
 * @param p1 mirror number, casted as uint32_t
 * @param p2 unused
 * @param p3 unused
 */
static void
motors_auto_homing_thread(void *p1, void *p2, void *p3)
{
    UNUSED_PARAMETER(p2);
    UNUSED_PARAMETER(p3);

    ret_code_t err_code = RET_SUCCESS;
    uint32_t motor = (uint32_t)p1;
    uint32_t status = 0;
    uint16_t last_stall_guard_values[2] = {0x0};
    size_t last_stall_guard_index = 0;
    int32_t timeout = 0;
    motors_refs[motor].auto_homing_state = AH_UNINIT;
    uint32_t loop_count = 0;
    uint32_t loop_count_last_step = 0;
    int32_t first_direction = 1;
    uint16_t sg = 0;
    uint16_t avg = 0;
    bool stall_detected = false;
    int32_t attempt = 0;

    LOG_INF("Initializing mirror %u", motor);
    reset_irun_sgt(motor);

    while (attempt < 2 && motors_refs[motor].auto_homing_state != AH_SUCCESS) {
        status = motor_controller_spi_read(
            spi_bus_controller, TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
        sg = status & 0x1FF;
        avg =
            (last_stall_guard_values[0] / 2) +
            (last_stall_guard_values[1] / 2) +
            ((last_stall_guard_values[0] % 2 + last_stall_guard_values[1] % 2) /
             2);
        stall_detected = false;

        LOG_DBG("Status %d 0x%08x, SG=%u, state %u", motor, status, sg,
                motors_refs[motor].auto_homing_state);
        if (!(status & MOTOR_DRV_STATUS_STANDSTILL) &&
            (motors_refs[motor].auto_homing_state == AH_LOOKING_FIRST_END ||
             motors_refs[motor].auto_homing_state == AH_GO_OTHER_END)) {
            if (sg < (avg * 0.75)) {
                LOG_DBG("Motor %u stall detection, avg %u, sg %u", motor, avg,
                        sg);
                stall_detected = true;
            }
            last_stall_guard_values[last_stall_guard_index] = sg;
            last_stall_guard_index = (1 - last_stall_guard_index);
        } else {
            memset(last_stall_guard_values, 0x00,
                   sizeof(last_stall_guard_values));
        }

        switch (motors_refs[motor].auto_homing_state) {

        case AH_UNINIT: {
            // reset values
            err_code = RET_SUCCESS;
            timeout = AUTOHOMING_TIMEOUT_LOOP_COUNT;

            // VSTART
            motor_controller_spi_write(spi_bus_controller,
                                       TMC5041_REGISTERS[REG_IDX_VSTART][motor],
                                       0x0);

            // write xactual = 0
            motor_controller_spi_write(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor],
                0x0);

            // clear status by reading RAMP_STAT
            motor_controller_spi_read(
                spi_bus_controller,
                TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // move a bit towards one end <-
            motor_controller_spi_send_commands(
                spi_bus_controller, position_mode_initial_phase[motor],
                ARRAY_SIZE(position_mode_initial_phase[motor]));

            int32_t steps = ((int32_t)AUTOHOMING_AWAY_FROM_BARRIER_STEPS *
                             (int32_t)first_direction);
            LOG_WRN("Steps away from barrier: %i", steps);
            motor_controller_spi_write(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                (uint32_t)steps);

            motors_refs[motor].auto_homing_state = AH_INITIAL_SHIFT;
        } break;
        case AH_INITIAL_SHIFT:
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                // motor is away from mechanical barrier
                LOG_INF("Motor %u away from mechanical barrier", motor);

                // clear events
                // the motor can be re-enabled by reading RAMP_STAT
                motor_controller_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

                to_one_direction(motor, (first_direction != 1));

                motors_refs[motor].auto_homing_state = AH_LOOKING_FIRST_END;
                loop_count_last_step = loop_count;

                // before we continue we need to wait for the motor to
                // remove its stallguard flag
                uint32_t t = 200 / AUTOHOMING_POLL_DELAY_MS;
                do {
                    k_msleep(AUTOHOMING_POLL_DELAY_MS);
                    status = motor_controller_spi_read(
                        spi_bus_controller,
                        TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
                    LOG_DBG("Status %d 0x%08x", motor, status);
                } while (status & MOTOR_DRV_STATUS_STALLGUARD || --t == 0);
            }
            break;
        case AH_LOOKING_FIRST_END:
            if ((status & MOTOR_DRV_STATUS_STALLGUARD) || stall_detected ||
                timeout == 0) {
                // motor stall detection done by checking either:
                // - motor stopped by using sg_stop (status flag)
                // OR
                // - timeout==0 means the motor is blocked in end course (didn't
                // move at all, preventing sg_stop from working)

                // stop the motor (VMAX in velocity mode)
                motor_controller_spi_write(
                    spi_bus_controller, TMC5041_REGISTERS[REG_IDX_VMAX][motor],
                    0x0);

                motors_refs[motor].auto_homing_state = AH_WAIT_STANDSTILL;

                if (timeout == 0) {
                    LOG_WRN("Timeout while looking for first end on motor %d, "
                            "increasing stall detection sensitivity",
                            motor);

                    first_direction = -first_direction;
                    increase_stall_sensitivity(motor);
                    motors_refs[motor].auto_homing_state = AH_UNINIT;
                } else if ((loop_count - loop_count_last_step) *
                               AUTOHOMING_POLL_DELAY_MS <=
                           200) {
                    // check that the motor moved for at least 200ms
                    // if not, we might be stuck, retry procedure while changing
                    // direction

                    LOG_WRN(
                        "Motor %u stalls quickly, decrease stall sensitivity",
                        motor);

                    // invert directions for autohoming in order to make sure we
                    // are not stuck
                    first_direction = -first_direction;
                    decrease_stall_sensivity(motor);
                    motors_refs[motor].auto_homing_state = AH_UNINIT;
                } else {
                    LOG_INF("Motor %u stalled", motor);
                }
            }
            break;
        case AH_WAIT_STANDSTILL:
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                // Wait until the motor is in standstill again by polling the
                // actual velocity VACTUAL or checking vzero or the standstill
                // flag

                LOG_INF("Motor %u reached first end pos", motor);

                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0);

                // write xactual = 0
                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

                // clear events
                // the motor can be re-enabled by reading RAMP_STAT
                motor_controller_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

                to_one_direction(motor, (first_direction == 1));

                // before we continue we need to wait for the motor to move and
                // removes its stall detection flag
                // timeout after 1 second
                uint32_t t = 1000 / AUTOHOMING_POLL_DELAY_MS;
                do {
                    k_msleep(AUTOHOMING_POLL_DELAY_MS);
                    status = motor_controller_spi_read(
                        spi_bus_controller,
                        TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
                    LOG_DBG("Status %d 0x%08x", motor, status);
                } while (status & MOTOR_DRV_STATUS_STALLGUARD || --t == 0);

                if (status & MOTOR_DRV_STATUS_STALLGUARD) {
                    LOG_ERR("Motor %u stalled when trying to reach other end",
                            motor);

                    motors_refs[motor].auto_homing_state = AH_FAIL;
                    err_code = RET_ERROR_INVALID_STATE;
                    break;
                }

                motors_refs[motor].auto_homing_state = AH_GO_OTHER_END;
            }
            break;
        case AH_GO_OTHER_END:
            if ((status & MOTOR_DRV_STATUS_STALLGUARD) || stall_detected ||
                timeout == 0) {

                if (timeout == 0) {
                    LOG_ERR("Timeout to other end");

                    motors_refs[motor].auto_homing_state = AH_FAIL;
                    err_code = RET_ERROR_INVALID_STATE;
                    break;
                }

                // stop the motor (VMAX in velocity mode)
                motor_controller_spi_write(
                    spi_bus_controller, TMC5041_REGISTERS[REG_IDX_VMAX][motor],
                    0x0);

                motor_controller_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

                k_msleep(100);

                // read current position
                int32_t x = (int32_t)motor_controller_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
                LOG_INF("Motor %u reached other end, pos %d", motor, x);

                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0);

                // verify that motor moved at least
                // `motors_full_course_minimum_steps`
                if ((uint32_t)abs(x) <
                    motors_full_course_minimum_steps[motor]) {
                    LOG_ERR(
                        "Motor %u range: %u microsteps, must be more than %u",
                        motor, abs(x),
                        abs((int32_t)motors_full_course_minimum_steps[motor] *
                            first_direction));

                    motors_refs[motor].auto_homing_state = AH_FAIL;
                    err_code = RET_ERROR_INVALID_STATE;
                    break;
                }

                motors_refs[motor].auto_homing_state = AH_SUCCESS;

                // write xactual = 0
                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);
                motors_refs[motor].steps_at_center_position = (-x / 2);
                motors_refs[motor].full_course = abs(x);

                uint32_t angle_millid =
                    2 *
                    calculate_millidegrees_from_center_position(
                        (int32_t)(motors_refs[motor].full_course / 2), motor);
                LOG_INF("Motor %u: range: %u millidegrees = %u usteps; x0: %d "
                        "usteps",
                        motor, angle_millid, motors_refs[motor].full_course,
                        motors_refs[motor].steps_at_center_position);

                MotorRange range = {
                    .which_motor = (motor == MOTOR_THETA_ANGLE
                                        ? MotorRange_Motor_VERTICAL_THETA
                                        : MotorRange_Motor_HORIZONTAL_PHI),
                    .range_microsteps = motors_refs[motor].full_course,
                    .range_millidegrees = angle_millid};
                publish_new(&range, sizeof(range), McuToJetson_motor_range_tag,
                            CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

                // go to middle position
                // setting in positioning mode after this loop
                // will drive the motor
                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                    motors_refs[motor].steps_at_center_position);
                break;
            }
            break;
        case AH_SUCCESS:
            break;
        case AH_FAIL:
            // - full range not detected
            // - stall detected far from second end
            motors_refs[motor].auto_homing_state = AH_UNINIT;
            reset_irun_sgt(motor);
            attempt++;
            break;
        }

        --timeout;
        loop_count += 1;
        k_msleep(AUTOHOMING_POLL_DELAY_MS);
    }

    // in any case, we want the motor to be in positioning mode
    motor_controller_spi_send_commands(
        spi_bus_controller, position_mode_full_speed[motor],
        ARRAY_SIZE(position_mode_full_speed[motor]));

    // keep auto-homing state
    motors_refs[motor].motor_state = err_code;

    if (err_code) {
        // todo raise event motor issue
    }

    if (motor == MOTOR_THETA_ANGLE) {
        motors_refs[motor].angle_millidegrees =
            MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES;
    } else {
        motors_refs[motor].angle_millidegrees =
            MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES;
    }

    k_sem_give(homing_in_progress_sem + motor);
}

ret_code_t
mirror_auto_homing_stall_detection(motor_t motor, struct k_thread **thread_ret)
{
    if (motor >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (k_sem_take(&homing_in_progress_sem[motor], K_NO_WAIT) == -EBUSY) {
        LOG_WRN("Motor %u auto-homing already in progress", motor);
        return RET_ERROR_BUSY;
    }

    if (motor == MOTOR_PHI_ANGLE) {
        if (thread_ret) {
            *thread_ret = &thread_data_motor_phi;
        }
        k_tid_t tid = k_thread_create(
            &thread_data_motor_phi, stack_area_motor_phi_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_phi_init),
            motors_auto_homing_thread, (void *)MOTOR_PHI_ANGLE, NULL, NULL,
            THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
        k_thread_name_set(tid, "mirror_ah_phi_stalldetect");
    } else {
        if (thread_ret) {
            *thread_ret = &thread_data_motor_theta;
        }
        k_tid_t tid = k_thread_create(
            &thread_data_motor_theta, stack_area_motor_theta_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_theta_init),
            motors_auto_homing_thread, (void *)MOTOR_THETA_ANGLE, NULL, NULL,
            THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
        k_thread_name_set(tid, "mirror_ah_theta_stalldetect");
    }

    return RET_SUCCESS;
}

#endif

bool
mirror_homed_successfully(void)
{
    return motors_refs[MOTOR_PHI_ANGLE].motor_state == RET_SUCCESS &&
           motors_refs[MOTOR_THETA_ANGLE].motor_state == RET_SUCCESS;
}

static void
mirror_auto_homing_one_end_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    uint32_t motor = (uint32_t)p1;
    uint32_t status = 0;
    int32_t timeout = AUTOHOMING_TIMEOUT_LOOP_COUNT;

    motors_refs[motor].auto_homing_state = AH_UNINIT;
    while (motors_refs[motor].auto_homing_state != AH_SUCCESS && timeout) {
        status = motor_controller_spi_read(
            spi_bus_controller, TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);

        LOG_DBG("Status %d 0x%08x, state %u", motor, status,
                motors_refs[motor].auto_homing_state);

        switch (motors_refs[motor].auto_homing_state) {
        case AH_UNINIT: {
            // write xactual = 0
            motor_controller_spi_write(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor],
                0x0);

            motor_controller_spi_send_commands(
                spi_bus_controller, position_mode_full_speed[motor],
                ARRAY_SIZE(position_mode_full_speed[motor]));
            int32_t steps = -motors_full_course_maximum_steps[motor];
            LOG_INF("Steps to one end: %i", steps);
            motor_controller_spi_write(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                steps);
            motors_refs[motor].auto_homing_state = AH_LOOKING_FIRST_END;
        } break;

        case AH_LOOKING_FIRST_END: {
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                // write xactual = 0
                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

                motors_refs[motor].steps_at_center_position =
                    (int32_t)motors_center_from_end_steps[motor];
                motors_refs[motor].full_course = motors_full_range_steps[motor];

                // go to middle position
                motor_controller_spi_write(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                    motors_refs[motor].steps_at_center_position);

                motors_refs[motor].auto_homing_state = AH_WAIT_STANDSTILL;
            }
        } break;
        case AH_INITIAL_SHIFT:
            break;
        case AH_WAIT_STANDSTILL: {
            if (status & MOTOR_DRV_STATUS_STANDSTILL) {
                uint32_t angle_range_millidegrees =
                    2 * calculate_millidegrees_from_center_position(
                            motors_refs[motor].full_course / 2, motor);
                LOG_INF("Motor %u, x0: center: %d microsteps, range: %u "
                        "millidegrees",
                        motor, motors_refs[motor].steps_at_center_position,
                        angle_range_millidegrees);

                orb_mcu_main_MotorRange range = {
                    .which_motor =
                        (motor == MOTOR_THETA_ANGLE
                             ? orb_mcu_main_MotorRange_Motor_VERTICAL_THETA
                             : orb_mcu_main_MotorRange_Motor_HORIZONTAL_PHI),
                    .range_microsteps = motors_refs[motor].full_course,
                    .range_millidegrees = angle_range_millidegrees};
                publish_new(&range, sizeof(range),
                            orb_mcu_main_McuToJetson_motor_range_tag,
                            CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);

                motors_refs[motor].auto_homing_state = AH_SUCCESS;
            }
        } break;
        case AH_GO_OTHER_END:
            break;
        case AH_SUCCESS:
            break;
        case AH_FAIL:
            break;
        }

        --timeout;
        k_msleep(AUTOHOMING_POLL_DELAY_MS);
    }

    // in any case, we want the motor to be in positioning mode
    motor_controller_spi_send_commands(
        spi_bus_controller, position_mode_full_speed[motor],
        ARRAY_SIZE(position_mode_full_speed[motor]));

    // keep auto-homing state
    if (timeout == 0) {
        motors_refs[motor].motor_state = RET_ERROR_INVALID_STATE;
    } else {
        motors_refs[motor].motor_state = RET_SUCCESS;
    }

    if (motor == MOTOR_THETA_ANGLE) {
        motors_refs[motor].angle_millidegrees =
            MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES;
    } else {
        motors_refs[motor].angle_millidegrees =
            MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES;
    }

    k_sem_give(homing_in_progress_sem + motor);
}

ret_code_t
mirror_auto_homing_one_end(motor_t motor)
{
    if (motor >= MOTORS_COUNT) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (k_sem_take(&homing_in_progress_sem[motor], K_NO_WAIT) == -EBUSY) {
        LOG_WRN("Motor %u auto-homing already in progress", motor);
        return RET_ERROR_BUSY;
    }

    if (motor == MOTOR_PHI_ANGLE) {
        // On a cold POR/boot of the Orb; going from a disconnected or
        // discharged battery to connected/charged, it takes time for the power
        // rails to come up and stabilize for the LM25117 buck controller and
        // subsequently the TMC5041 stepper motor driver to be able to act on
        // SPI commands
        // TODO ** TUNE DOWN DELAYS AFTER TALKING TO HARDWARE TEAM **
        k_timeout_t delay_phi = (motors_refs[MOTOR_PHI_ANGLE].motor_state ==
                                         RET_ERROR_NOT_INITIALIZED
                                     ? K_MSEC(2000)
                                     : K_NO_WAIT);
        k_tid_t tid = k_thread_create(
            &thread_data_motor_phi, stack_area_motor_phi_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_phi_init),
            mirror_auto_homing_one_end_thread, (void *)MOTOR_PHI_ANGLE, NULL,
            NULL, THREAD_PRIORITY_MOTORS_INIT, 0, delay_phi);
        k_thread_name_set(tid, "motor_ah_phi_one_end");
    } else {
        k_timeout_t delay_theta = (motors_refs[MOTOR_THETA_ANGLE].motor_state ==
                                           RET_ERROR_NOT_INITIALIZED
                                       ? K_MSEC(4000)
                                       : K_NO_WAIT);

        k_tid_t tid = k_thread_create(
            &thread_data_motor_theta, stack_area_motor_theta_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_theta_init),
            mirror_auto_homing_one_end_thread, (void *)MOTOR_THETA_ANGLE, NULL,
            NULL, THREAD_PRIORITY_MOTORS_INIT, 0, delay_theta);
        k_thread_name_set(tid, "motor_ah_theta_one_end");
    }

    return RET_SUCCESS;
}

bool
mirror_auto_homing_in_progress()
{
    return (k_sem_count_get(&homing_in_progress_sem[MOTOR_THETA_ANGLE]) == 0 ||
            k_sem_count_get(&homing_in_progress_sem[MOTOR_PHI_ANGLE]) == 0);
}

uint32_t
mirror_get_phi_angle_millidegrees(void)
{
    return motors_refs[MOTOR_PHI_ANGLE].angle_millidegrees;
}

uint32_t
mirror_get_theta_angle_millidegrees(void)
{
    return motors_refs[MOTOR_THETA_ANGLE].angle_millidegrees;
}

ret_code_t
mirror_init(void)
{
    int err_code;
    uint32_t read_value;

    if (!device_is_ready(spi_bus_controller)) {
        LOG_ERR("motion controller SPI device not ready");
        return RET_ERROR_INVALID_STATE;
    } else {
        LOG_INF("Motion controller SPI ready");
    }

#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    // write TMC5041_REG_GCONF to 0x300 to invert motor direction (bit 8 & 9)
    motor_controller_spi_write(spi_bus_controller, TMC5041_REG_GCONF, 0x300);
#endif

    read_value =
        motor_controller_spi_read(spi_bus_controller, TMC5041_REG_GCONF);
    LOG_INF("GCONF: 0x%08x", read_value);
    k_msleep(10);

    read_value = motor_controller_spi_read(spi_bus_controller, REG_INPUT);
    LOG_INF("Input: 0x%08x", read_value);
    uint8_t ic_version = (read_value >> 24 & 0xFF);

    if (ic_version != TMC5041_IC_VERSION) {
        LOG_ERR("Error reading TMC5041");
        return RET_ERROR_OFFLINE;
    }

    err_code = k_sem_init(&homing_in_progress_sem[MOTOR_PHI_ANGLE], 1, 1);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    err_code = k_sem_init(&homing_in_progress_sem[MOTOR_THETA_ANGLE], 1, 1);
    if (err_code) {
        ASSERT_SOFT(err_code);
        return RET_ERROR_INTERNAL;
    }

    motors_refs[MOTOR_PHI_ANGLE].motor_state = RET_ERROR_NOT_INITIALIZED;
    motors_refs[MOTOR_THETA_ANGLE].motor_state = RET_ERROR_NOT_INITIALIZED;

    // Set motor in positioning mode
    motor_controller_spi_send_commands(
        spi_bus_controller, position_mode_full_speed[MOTOR_PHI_ANGLE],
        ARRAY_SIZE(position_mode_full_speed[MOTOR_PHI_ANGLE]));
    motor_controller_spi_send_commands(
        spi_bus_controller, position_mode_full_speed[MOTOR_THETA_ANGLE],
        ARRAY_SIZE(position_mode_full_speed[MOTOR_THETA_ANGLE]));

    // auto-home after boot
    err_code = mirror_auto_homing_one_end(MOTOR_PHI_ANGLE);
    ASSERT_SOFT(err_code);
    err_code = mirror_auto_homing_one_end(MOTOR_THETA_ANGLE);
    ASSERT_SOFT(err_code);

    k_work_init(&theta_angle_set_work_item.work,
                mirror_angle_theta_work_wrapper);
    k_work_init(&phi_angle_set_work_item.work, mirror_angle_phi_work_wrapper);

    k_work_queue_init(&mirror_work_queue);
    k_work_queue_start(&mirror_work_queue, stack_area_mirror_work_queue,
                       K_THREAD_STACK_SIZEOF(stack_area_mirror_work_queue),
                       THREAD_PRIORITY_MOTORS_INIT, NULL);

    return RET_SUCCESS;
}
