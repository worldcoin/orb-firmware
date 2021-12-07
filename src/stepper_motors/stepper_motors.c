#include "stepper_motors.h"
#include <device.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <app_config.h>
#include <compilers.h>
#include <logging/log.h>
#include <stdlib.h>
LOG_MODULE_REGISTER(stepper_motors);

K_THREAD_STACK_DEFINE(stack_area_motor_horizontal_init, 256);
K_THREAD_STACK_DEFINE(stack_area_motor_vertical_init, 256);
static struct k_thread thread_data_motor_horizontal;
static struct k_thread thread_data_motor_vertical;
static k_tid_t tid_autohoming[MOTOR_COUNT] = {0};

// SPI w/ TMC5041
#define SPI_DEVICE          DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ  0
#define WRITE (1 << 7)

#define TMC5041_IC_VERSION 0x10

#define TMC5041_REG_GCONF 0x00
#define REG_INPUT         0x04

typedef enum tmc5041_registers_e {
    REG_IDX_XACTUAL,
    REG_IDX_VSTART,
    REG_IDX_VMAX,
    REG_IDX_XTARGET,
    REG_IDX_SW_MODE,
    REG_IDX_RAMP_STAT,
    REG_IDX_COOLCONF,
    REG_IDX_DRV_STATUS,
    REG_IDX_COUNT
} tmc5041_registers_t;

const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][MOTOR_COUNT] = {
    {0x21, 0x41}, // XACTUAL
    {0x23, 0x43}, // VSTART
    {0x27, 0x47}, // VMAX
    {0x2D, 0x4D}, // XTARGET
    {0x34, 0x54}, // SW_MODE
    {0x35, 0x55}, // RAMP_STAT
    {0x6D, 0x7D}, // COOLCONF
    {0x6F, 0x7F}, // DRV_STATUS
};

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

// Motors configuration
#define MOTOR_INIT_VMAX 80000
#define MOTOR_FS_VMAX   800000

const uint32_t motors_full_course_steps[MOTOR_COUNT] = {(300 * 256),
                                                        (500 * 256)};
const uint8_t motors_stall_guard_threshold[MOTOR_COUNT] = {5, 5};

typedef struct {
    int x0; // step at x=0 (middle position)
    int x_current;
    uint32_t motor_state;
} motors_refs_t;

static motors_refs_t motors_refs[MOTOR_COUNT] = {0};

const uint64_t motor_init_for_velocity_mode[MOTOR_COUNT][11] = {
    {0x8000000008,
     0xEC000100C5, // 0x6C CHOPCONF
     0xB000011000, // IHOLD_IRUN reg, bytes from left to right: I_HOLD=0,
                   // I_RUN=16, IHOLDDELAY=1
     0xAC00002710,
     0x90000401C8, // PWMCONF
     0xB200061A80,
     0xB100000000 +
         (MOTOR_INIT_VMAX / 2), // VCOOLTHRS: StallGuard enabled when motor
                                // reaches that velocity. Let's use ~VMAX/2
     0xA600001388,
     0xA700000000 + MOTOR_INIT_VMAX, // VMAX
     0xB400000000 | (1 << 10),       // SW_MODE sg_stop
     0xA000000001},
    {0x8000000008,
     0xFC000100C5, // 0x6C CHOPCONF
     0xD000011000, // IHOLD_IRUN reg, bytes from left to right: I_HOLD=0,
                   // I_RUN=16, IHOLDDELAY=1
     0xCC00002710,
     0x98000401C8, // PWMCONF
     0xD200061A80,
     0xD100000000 +
         (MOTOR_INIT_VMAX / 2), // VCOOLTHRS: StallGuard enabled when motor
                                // reaches that velocity. Let's use ~VMAX/2
     0xC600001388,
     0xC700000000 + MOTOR_INIT_VMAX, // VMAX
     0xD400000000 | (1 << 10),       // SW_MODE sg_stop
     0xC000000001}};

const uint64_t position_mode_initial_phase[MOTOR_COUNT][8] = {
    {
        0xA4000003E8, // A1 first acceleration
        0xA50000C350, // V1 Acceleration threshold, velocity V1
        0xA6000001F4, // Acceleration above V1
        0xA700000000 + MOTOR_INIT_VMAX, // VMAX
        0xA8000002BC,                   // DMAX Deceleration above V1
        0xAA00000578,                   // D1 Deceleration below V1
        0xAB0000000A,                   // VSTOP stop velocity
        0xA000000000,                   // RAMPMODE = 0 position move
    },
    {
        0xC4000003E8, // A1 first acceleration
        0xC50000C350, // V1 Acceleration threshold, velocity V1
        0xC6000001F4, // Acceleration above V1
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX
        0xC8000002BC,                   // DMAX Deceleration above V1
        0xCA00000578,                   // D1 Deceleration below V1
        0xCB0000000A,                   // VSTOP stop velocity
        0xC000000000,                   // RAMPMODE = 0 position move
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

    int32_t steps_per_degree = (int32_t)motors_full_course_steps[motor] / 40;
    int32_t xtarget = (d_from_center * steps_per_degree) / 1000;
    xtarget = xtarget + motors_refs[motor].x0;

    LOG_INF("Setting motor %u to: %d milli-degrees (%d)", motor, d_from_center,
            xtarget);

    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor], xtarget);

    return RET_SUCCESS;
}

ret_code_t
motors_angle_horizontal(int32_t angle_millidegrees)
{
    if (angle_millidegrees > 65000 || angle_millidegrees < 25000) {
        LOG_ERR("Accepted range is [25000;65000], got %d", angle_millidegrees);
        return RET_ERROR_INVALID_PARAM;
    }

    // recenter
    int32_t m_degrees_from_center = angle_millidegrees - 45000;

    return motors_set_angle_relative(m_degrees_from_center, MOTOR_HORIZONTAL);
}

ret_code_t
motors_angle_vertical(int32_t angle_millidegrees)
{
    if (angle_millidegrees > 20000 || angle_millidegrees < -20000) {
        LOG_ERR("Accepted range is [-20000;20000], got %d", angle_millidegrees);
        return RET_ERROR_INVALID_PARAM;
    }

    return motors_set_angle_relative(angle_millidegrees, MOTOR_VERTICAL);
}

/**
 * @brief Perform auto-homing
 *
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

    LOG_INF("Initializing motor %u", motor);

    // VSTART
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_VSTART][motor], 0x0);

    // COOLCONF, set SGT to offset StallGuard value
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_COOLCONF][motor],
                    (motors_stall_guard_threshold[motor] << 16) |
                        (1 << 24) /* enable SG filter */);

    // start in one direction until stall is detected
    motor_spi_send_commands(spi_bus_controller,
                            motor_init_for_velocity_mode[motor],
                            ARRAY_SIZE(motor_init_for_velocity_mode[motor]));

    uint32_t status;
    int32_t x_first_end = 0;
    // timeout to detect first end
    // in unit of 250ms = 5 seconds
    // = maximum time to go from one side to the other during auto-homing + some
    // delay
    uint32_t timeout = 20;

    while (1) {
        status = motor_spi_read(spi_bus_controller,
                                TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
        LOG_DBG("Status %d 0x%08x", motor, status);

        // motor stall detection done by checking either:
        // - motor stopped by using sg_stop (status flag)
        // OR
        // - timeout==0 means the motor is blocked in end course (didn't move at
        // all, preventing sg_stop from working)
        if (x_first_end == 0 && (status & (1 << 24) || --timeout == 0)) {
            if (timeout == 0) {
                LOG_WRN("Timeout while looking for first end on motor %d",
                        motor);
            }

            // stop motor
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_VMAX][motor], 0x0);

            // read current position at least twice to make sure motor is
            // stopped
            x_first_end = 0;
            int32_t x = 0;
            do {
                x_first_end = x;
                x = (int32_t)motor_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
                k_msleep(10);
            } while (x_first_end != x);

            LOG_INF("Motor %u stalled. First end pos: %d", motor, x_first_end);

            // disable sg_stop
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0x0);

            // clear events
            // the motor can be re-enabled by reading RAMP_STAT
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // go to the other end using position mode
            motor_spi_send_commands(
                spi_bus_controller, position_mode_initial_phase[motor],
                ARRAY_SIZE(position_mode_initial_phase[motor]));

            // ready to move, let's go to the other end
            motor_spi_write(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                (uint32_t)(x_first_end - motors_full_course_steps[motor]));

            // before we continue we need to wait for the motor to move and
            // removes its stall detection flag
            uint32_t t = 10;
            do {
                k_msleep(100);
                status = motor_spi_read(
                    spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
                LOG_DBG("Status %d 0x%08x", motor, status);
            } while (status & (1 << 24) || --t == 0);

            if (status & (1 << 24)) {
                LOG_ERR("Motor %u stalled when trying to reach other end",
                        motor);
                err_code = RET_ERROR_INVALID_STATE;
                break;
            }

            continue;
        }

        if (x_first_end != 0 && status & (1 << 31)) {
            motor_spi_read(spi_bus_controller,
                           TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // read current position
            int32_t x = (int32_t)motor_spi_read(
                spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
            LOG_INF("Motor %u reached other end, pos %d", motor, x);

            if (abs(x - (x_first_end - motors_full_course_steps[motor])) >
                256) {
                LOG_INF("Didn't reached target: x=%d, should be ~ %d", x,
                        (x_first_end - motors_full_course_steps[motor]));

                err_code = RET_ERROR_INVALID_STATE;
                break;
            }

            motors_refs[motor].x0 = x + (motors_full_course_steps[motor] / 2);

            LOG_INF("Motor %u, x0: %d", motor, motors_refs[motor].x0);

            // go to middle position
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                            motors_refs[motor].x0);

            motor_spi_send_commands(
                spi_bus_controller, position_mode_full_speed[motor],
                ARRAY_SIZE(position_mode_full_speed[motor]));

            break;
        }

        k_msleep(250);
    }

    // keep auto-homing state
    motors_refs[motor].motor_state = err_code;

    if (err_code) {
        // todo raise event motor issue
    }

    tid_autohoming[motor] = NULL;
}

ret_code_t
motors_auto_homing(motor_t motor)
{
    __ASSERT(motor <= MOTOR_COUNT, "Wrong motor number");

    if (tid_autohoming[motor] != NULL) {
        LOG_ERR("Motor %u auto-homing already in progress", motor);
        return RET_ERROR_FORBIDDEN;
    }

    if (motor == MOTOR_HORIZONTAL) {
        tid_autohoming[motor] = k_thread_create(
            &thread_data_motor_horizontal, stack_area_motor_horizontal_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_horizontal_init),
            motors_auto_homing_thread, (void *)MOTOR_HORIZONTAL, NULL, NULL,
            THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
    } else {
        tid_autohoming[motor] = k_thread_create(
            &thread_data_motor_vertical, stack_area_motor_vertical_init,
            K_THREAD_STACK_SIZEOF(stack_area_motor_vertical_init),
            motors_auto_homing_thread, (void *)MOTOR_VERTICAL, NULL, NULL,
            THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
    }

    if (tid_autohoming[motor] == NULL) {
        return RET_ERROR_INTERNAL;
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

    motors_refs[MOTOR_HORIZONTAL].motor_state = RET_ERROR_NOT_INITIALIZED;
    motors_refs[MOTOR_VERTICAL].motor_state = RET_ERROR_NOT_INITIALIZED;

    motors_auto_homing(MOTOR_HORIZONTAL);
    motors_auto_homing(MOTOR_VERTICAL);

    return RET_SUCCESS;
}
