#include "stepper_motors.h"
#include <device.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <app_config.h>
#include <compilers.h>
#include <logging/log.h>
#include <math.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(stepper_motors);

K_THREAD_STACK_DEFINE(stack_area_motor_horizontal_init, 600);
K_THREAD_STACK_DEFINE(stack_area_motor_vertical_init, 600);
static struct k_thread thread_data_motor_horizontal;
static struct k_thread thread_data_motor_vertical;
static struct k_sem homing_in_progress_sem[MOTOR_COUNT];

// SPI w/ TMC5041
#define SPI_DEVICE          DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ  0
#define WRITE (1 << 7)

static struct spi_config spi_cfg = {.frequency = 1000000,
                                    .operation = SPI_WORD_SET(8) |
                                                 SPI_OP_MODE_MASTER |
                                                 SPI_MODE_CPOL | SPI_MODE_CPHA,
                                    .cs = SPI_CS_CONTROL_PTR_DT(SPI_DEVICE, 2)};
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
#define MOTOR_INIT_VMAX 200000
#define MOTOR_INIT_AMAX (MOTOR_INIT_VMAX / 20)
#define MOTOR_FS_VMAX   800000
#define IHOLDDELAY      (1 << 16)

// initial values [IRUN, SGT]
const uint8_t motor_irun_sgt[MOTOR_COUNT][2] = {
    {0x12, 5}, // vertical
    {0x13, 6}, // horizontal
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

const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][MOTOR_COUNT] = {
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

const uint32_t motors_full_course_minimum_steps[MOTOR_COUNT] = {(300 * 256),
                                                                (300 * 256)};
const float motors_arm_length[MOTOR_COUNT] = {12.0, 18.71};

const uint32_t steps_per_mm = 12800; // 1mm / 0.4mm (pitch) * (360° / 18° (per
                                     // step)) * 256 micro-steps

enum motor_autohoming_state {
    AH_UNINIT,
    AH_INITIAL_SHIFT,
    AH_LOOKING_FIRST_END,
    AH_WAIT_STANDSTILL,
    AH_GO_OTHER_END,
    AH_SUCCESS,
    AH_FAIL,
};
typedef struct {
    int32_t x0; // step at x=0 (middle position)
    int32_t full_course;
    uint8_t velocity_mode_current;
    uint8_t stall_guard_threshold;
    enum motor_autohoming_state auto_homing_state;
    uint32_t motor_state;
} motors_refs_t;

static motors_refs_t motors_refs[MOTOR_COUNT] = {0};

#define SW_MODE_SG_ENABLE (1 << 10)

/// One direction with stall guard detection
/// Velocity mode
const uint64_t motor_init_for_velocity_mode[MOTOR_COUNT][8] = {
    // Vertical motor
    {
        0xEC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xAC00000010, // TZEROWAIT
        0x90000401C8, // PWMCONF
        0xB200061A80,
        0xB100000000 + (MOTOR_INIT_VMAX * 9 /
                        10), // VCOOLTHRS: StallGuard enabled when motor
                             // reaches that velocity
        0xA600000000 + MOTOR_INIT_AMAX, // AMAX = acceleration and deceleration
                                        // in velocity mode
        0xA700000000 + MOTOR_INIT_VMAX, // VMAX target velocity
        0xB400000000 |
            SW_MODE_SG_ENABLE, // SW_MODE sg_stop enable stop using stallguard
    },

    // Horizontal motor
    {
        0xFC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xCC00000010, // TZEROWAIT
        0x98000401C8, // PWMCONF
        0xD200061A80,
        0xD100000000 + (MOTOR_INIT_VMAX * 9 /
                        10), // VCOOLTHRS: StallGuard enabled when motor
                             // reaches that velocity
        0xC600000000 + MOTOR_INIT_AMAX, // AMAX = acceleration and deceleration
                                        // in velocity mode
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX target velocity
        0xD400000000 |
            SW_MODE_SG_ENABLE, // SW_MODE sg_stop enable stop using stallguard
    }};                        // RAMPMODE velocity mode to +VMAX using AMAX

const uint64_t position_mode_initial_phase[MOTOR_COUNT][10] = {
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
    {
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

const uint64_t position_mode_full_speed[MOTOR_COUNT][8] = {
    {
        0xA400008000, // A1 first acceleration
        0xA500000000 +
            MOTOR_FS_VMAX * 3 / 4,    // V1 Acceleration threshold, velocity V1
        0xA600001000,                 // Acceleration above V1
        0xA700000000 + MOTOR_FS_VMAX, // VMAX
        0xA800001000,                 // DMAX Deceleration above V1
        0xAA00008000,                 // D1 Deceleration below V1
        0xAB00000010,                 // VSTOP stop velocity
        0xA000000000,                 // RAMPMODE = 0 position move
    },
    {
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

// Adding 2 to IRUN adds 1 to SGT value
static void
increase_irun_current(motor_t motor)
{
    if (motors_refs[motor].velocity_mode_current < 0x1E) {
        motors_refs[motor].velocity_mode_current += 2;
        motors_refs[motor].stall_guard_threshold += 1;
    } else {
        LOG_ERR("Cannot increase current");
    }
}

static void
decrease_irun_current(motor_t motor)
{
    if (motors_refs[motor].velocity_mode_current > 0x0E) {
        motors_refs[motor].velocity_mode_current -= 2;
        motors_refs[motor].stall_guard_threshold -= 1;
    } else {
        LOG_ERR("Cannot decrease current");
    }
}
static void
reset_irun_sgt(motor_t motor)
{
    motors_refs[motor].velocity_mode_current = motor_irun_sgt[motor][0];
    motors_refs[motor].stall_guard_threshold = motor_irun_sgt[motor][1];
}

static void
motor_spi_send_commands(const struct device *spi_bus_controller,
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

static void
motor_spi_write(const struct device *spi_bus_controller, uint8_t reg,
                uint32_t value)
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
    __ASSERT(ret == 0, "Error SPI transceive");
}

static uint32_t
motor_spi_read(const struct device *spi_bus_controller, uint8_t reg)
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
    __ASSERT(ret == 0, "Error SPI transceive");

    memset(rx_buffer, 0, sizeof(rx_buffer));

    // second, read data
    ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
    __ASSERT(ret == 0, "Error SPI transceive");

    uint32_t read_value = (rx_buffer[1] << 24 | rx_buffer[2] << 16 |
                           rx_buffer[3] << 8 | rx_buffer[4] << 0);

    return read_value;
}

/// Set relative angle in millidegrees from the center position
/// \param d_from_center millidegrees from center
/// \param motor motor 0 or 1
/// \return
static ret_code_t
motors_set_angle_relative(int32_t d_from_center, motor_t motor)
{
    __ASSERT(motor < MOTOR_COUNT, "Motor number not handled");

    if (motors_refs[motor].motor_state) {
        return motors_refs[motor].motor_state;
    }

    float millimeters =
        sinf((float)d_from_center * M_PI / 360000.0) * motors_arm_length[motor];
    int32_t steps = (int32_t)roundf(millimeters * (float)steps_per_mm);
    int32_t xtarget = motors_refs[motor].x0 + steps;

    LOG_INF("Setting motor %u to: %d milli-degrees (%d)", motor, d_from_center,
            xtarget);

    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor], xtarget);

    return RET_SUCCESS;
}

ret_code_t
motors_angle_horizontal(int32_t angle_millidegrees)
{
    if (angle_millidegrees > MOTORS_ANGLE_HORIZONTAL_MAX ||
        angle_millidegrees < MOTORS_ANGLE_HORIZONTAL_MIN) {
        LOG_ERR("Accepted range is [%u;%u], got %d",
                MOTORS_ANGLE_HORIZONTAL_MIN, MOTORS_ANGLE_HORIZONTAL_MAX,
                angle_millidegrees);
        return RET_ERROR_INVALID_PARAM;
    }

    // recenter
    int32_t m_degrees_from_center = angle_millidegrees - 45000;

    return motors_set_angle_relative(m_degrees_from_center, MOTOR_HORIZONTAL);
}

ret_code_t
motors_angle_vertical(int32_t angle_millidegrees)
{
    if (angle_millidegrees > MOTORS_ANGLE_VERTICAL_MAX ||
        angle_millidegrees < MOTORS_ANGLE_VERTICAL_MIN) {
        LOG_ERR("Accepted range is [%d;%d], got %d", MOTORS_ANGLE_VERTICAL_MIN,
                MOTORS_ANGLE_VERTICAL_MAX, angle_millidegrees);
        return RET_ERROR_INVALID_PARAM;
    }

    return motors_set_angle_relative(angle_millidegrees, MOTOR_VERTICAL);
}

static void
to_one_direction(motor_t motor, bool positive_direction)
{
    uint8_t current = motors_refs[motor].velocity_mode_current;
    uint8_t sgt = motors_refs[motor].stall_guard_threshold;

    if (current >= 32) {
        APP_ASSERT(RET_ERROR_INVALID_PARAM);
    }

    // COOLCONF, set SGT to offset StallGuard value
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_COOLCONF][motor],
                    (sgt << 16) | (1 << 24) /* enable SG filter */);

    // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
    // IHOLD = 0
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_IHOLD_IRUN][motor],
                    IHOLDDELAY | (current << 8));

    // start velocity mode until stall is detected ->
    motor_spi_send_commands(spi_bus_controller,
                            motor_init_for_velocity_mode[motor],
                            ARRAY_SIZE(motor_init_for_velocity_mode[motor]));

    // let's go
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_RAMPMODE][motor],
                    positive_direction ? 1 : 2);
}

/**
 * @brief Perform auto-homing
 * See TMC5041 DATASHEET (Rev. 1.14 / 2020-JUN-12) page 58
 * This thread sets the motor state after auto-homing procedure
 *
 * @param p1 motor number, casted as uint32_t
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
    uint32_t status;

    reset_irun_sgt(motor);

    LOG_INF("Initializing motor %u", motor);

    // autohoming timeout to detect both ends = 5 seconds
    // = maximum time to go from one side to the other during auto-homing + some
    // delay
    const uint32_t loop_delay_ms = 50;
    int32_t timeout = 5000 / loop_delay_ms;

    motors_refs[motor].auto_homing_state = AH_UNINIT;
    uint32_t loop_count = 0;
    uint32_t loop_count_last_step = 0;
    int32_t first_direction = 1;

    while (1) {
        status = motor_spi_read(spi_bus_controller,
                                TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
        LOG_DBG("Status %d 0x%08x, SG=%u, state %u", motor, status,
                (status & 0x1F), motors_refs[motor].auto_homing_state);

        if (motors_refs[motor].auto_homing_state == AH_UNINIT) {
            // VSTART
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_VSTART][motor], 0x0);

            // write xactual = 0
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

            // clear status by reading RAMP_STAT
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // move a bit towards one end <-
            motor_spi_send_commands(
                spi_bus_controller, position_mode_initial_phase[motor],
                ARRAY_SIZE(position_mode_initial_phase[motor]));

            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                            (uint32_t)(25000 * first_direction));

            motors_refs[motor].auto_homing_state = AH_INITIAL_SHIFT;
        } else if (motors_refs[motor].auto_homing_state == AH_INITIAL_SHIFT &&
                   (status & MOTOR_DRV_STATUS_STANDSTILL)) {
            // motor is away from mechanical barrier

            // clear events
            // the motor can be re-enabled by reading RAMP_STAT
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            to_one_direction(motor, (first_direction != 1));

            motors_refs[motor].auto_homing_state = AH_LOOKING_FIRST_END;
            loop_count_last_step = loop_count;
        } else if (motors_refs[motor].auto_homing_state ==
                       AH_LOOKING_FIRST_END &&
                   ((status & MOTOR_DRV_STATUS_STALLGUARD) || timeout == 0)) {
            // motor stall detection done by checking either:
            // - motor stopped by using sg_stop (status flag)
            // OR
            // - timeout==0 means the motor is blocked in end course (didn't
            // move at all, preventing sg_stop from working)

            // stop the motor (VMAX in velocity mode)
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_VMAX][motor], 0x0);

            motors_refs[motor].auto_homing_state = AH_WAIT_STANDSTILL;

            if (timeout == 0) {
                LOG_WRN("Timeout while looking for first end on motor %d, "
                        "decreasing SGT",
                        motor);

                timeout = 5000 / loop_delay_ms;

                first_direction = -first_direction;
                decrease_irun_current(motor);
                motors_refs[motor].auto_homing_state = AH_UNINIT;
            } else if (loop_count - loop_count_last_step < 3) {
                // check that the motor moved for at least 200ms
                // if not, we might be stuck, retry procedure while changing
                // direction

                LOG_WRN("Seems like IRUN/SGT need tuning motor %u", motor);

                // invert directions for autohoming in order to make sure we are
                // not stuck
                first_direction = -first_direction;
                increase_irun_current(motor);
                motors_refs[motor].auto_homing_state = AH_UNINIT;
            }
        } else if (motors_refs[motor].auto_homing_state == AH_WAIT_STANDSTILL &&
                   status & MOTOR_DRV_STATUS_STANDSTILL) {
            // Wait until the motor is in standstill again by polling the
            // actual velocity VACTUAL or checking vzero or the standstill flag

            LOG_INF("Motor %u reached first end pos", motor);

            // stop the motor (VMAX in velocity mode)
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_VMAX][motor], 0x0);

            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0);

            k_msleep(100);

            // write xactual = 0
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);

            // clear events
            // the motor can be re-enabled by reading RAMP_STAT
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            to_one_direction(motor, (first_direction == 1));

            // before we continue we need to wait for the motor to move and
            // removes its stall detection flag
            uint32_t t = 1000 / loop_delay_ms;
            do {
                k_msleep(loop_delay_ms);
                status = motor_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
                LOG_DBG("Status %d 0x%08x", motor, status);
            } while (status & MOTOR_DRV_STATUS_STALLGUARD || --t == 0);

            if (status & (1 << 24)) {
                LOG_ERR("Motor %u stalled when trying to reach other end",
                        motor);
                err_code = RET_ERROR_INVALID_STATE;
                break;
            }

            motors_refs[motor].auto_homing_state = AH_GO_OTHER_END;
        } else if (motors_refs[motor].auto_homing_state == AH_GO_OTHER_END &&
                   ((status & MOTOR_DRV_STATUS_STALLGUARD) || timeout == 0)) {
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // stop the motor (VMAX in velocity mode)
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_VMAX][motor], 0x0);

            k_msleep(100);

            // read current position
            int32_t x = (int32_t)motor_spi_read(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
            LOG_INF("Motor %u reached other end, pos %d", motor, x);

            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0);

            // verify that motor moved at least
            // `motors_full_course_minimum_steps`
            if (abs(x) < motors_full_course_minimum_steps[motor]) {
                LOG_ERR(
                    "Motor %u didn't reach target: x=%d, should be at least %d",
                    motor, x,
                    (motors_full_course_minimum_steps[motor] *
                     first_direction));

                motors_refs[motor].auto_homing_state = AH_FAIL;
                err_code = RET_ERROR_INVALID_STATE;
                break;
            }

            motors_refs[motor].auto_homing_state = AH_SUCCESS;

            // write xactual = 0
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XACTUAL][motor], 0x0);
            motors_refs[motor].x0 = (-x / 2);
            motors_refs[motor].full_course = abs(x) / 2;

            LOG_INF("Motor %u, x0: %d", motor, motors_refs[motor].x0);

            // go to middle position
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                            motors_refs[motor].x0);

            motor_spi_send_commands(
                spi_bus_controller, position_mode_full_speed[motor],
                ARRAY_SIZE(position_mode_full_speed[motor]));

            break;
        } else if (timeout == 0) {
            LOG_ERR("TIMEOUT");
            break;
        }

        --timeout;
        loop_count += 1;
        k_msleep(loop_delay_ms);
    }

    // keep auto-homing state
    motors_refs[motor].motor_state = err_code;

    if (err_code) {
        // todo raise event motor issue
    }

    k_sem_give(homing_in_progress_sem + motor);
}

bool
motors_homed_successfully(void)
{
    return motors_refs[MOTOR_HORIZONTAL].motor_state == RET_SUCCESS &&
           motors_refs[MOTOR_VERTICAL].motor_state == RET_SUCCESS;
}

ret_code_t
motors_auto_homing(motor_t motor, struct k_thread **thread_ret)
{
    __ASSERT(motor <= MOTOR_COUNT, "Wrong motor number");

    if (k_sem_take(&homing_in_progress_sem[motor], K_NO_WAIT) == -EBUSY) {
        LOG_WRN("Motor %u auto-homing already in progress", motor);
        return RET_ERROR_BUSY;
    }

    if (motor == MOTOR_HORIZONTAL) {
        if (thread_ret) {
            *thread_ret = &thread_data_motor_horizontal;
        }
        k_thread_create(&thread_data_motor_horizontal,
                        stack_area_motor_horizontal_init,
                        K_THREAD_STACK_SIZEOF(stack_area_motor_horizontal_init),
                        motors_auto_homing_thread, (void *)MOTOR_HORIZONTAL,
                        NULL, NULL, THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
    } else {
        if (thread_ret) {
            *thread_ret = &thread_data_motor_vertical;
        }
        k_thread_create(&thread_data_motor_vertical,
                        stack_area_motor_vertical_init,
                        K_THREAD_STACK_SIZEOF(stack_area_motor_vertical_init),
                        motors_auto_homing_thread, (void *)MOTOR_VERTICAL, NULL,
                        NULL, THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
    }

    return RET_SUCCESS;
}

ret_code_t
motors_init(void)
{
    uint32_t read_value;

    if (!device_is_ready(spi_bus_controller)) {
        LOG_ERR("motion controller SPI device not ready");
        return RET_ERROR_BUSY;
    } else {
        LOG_INF("Motion controller SPI ready");
    }

    read_value = motor_spi_read(spi_bus_controller, TMC5041_REG_GCONF);
    LOG_INF("GCONF: 0x%08x", read_value);
    k_msleep(10);

    read_value = motor_spi_read(spi_bus_controller, REG_INPUT);
    LOG_INF("Input: 0x%08x", read_value);
    uint8_t ic_version = (read_value >> 24 & 0xFF);

    if (ic_version != TMC5041_IC_VERSION) {
        LOG_ERR("Error reading TMC5041");
        return RET_ERROR_INVALID_STATE;
    }

    k_sem_init(&homing_in_progress_sem[MOTOR_HORIZONTAL], 1, 1);
    k_sem_init(&homing_in_progress_sem[MOTOR_VERTICAL], 1, 1);

    motors_refs[MOTOR_HORIZONTAL].motor_state = RET_ERROR_NOT_INITIALIZED;
    motors_refs[MOTOR_VERTICAL].motor_state = RET_ERROR_NOT_INITIALIZED;

    motors_auto_homing(MOTOR_HORIZONTAL, NULL);
    motors_auto_homing(MOTOR_VERTICAL, NULL);

    return RET_SUCCESS;
}
