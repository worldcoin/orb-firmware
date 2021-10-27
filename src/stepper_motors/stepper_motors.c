#include "stepper_motors.h"
#include <device.h>
#include <zephyr.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>

#include <logging/log.h>
#include <stdlib.h>
#include <app_config.h>
#include <compilers.h>
LOG_MODULE_REGISTER(stepper_motors);

#define SPI_DEVICE DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

K_THREAD_STACK_DEFINE(stack_area_motor1_init, 256);
K_THREAD_STACK_DEFINE(stack_area_motor2_init, 256);
static struct k_thread thread_data_motor1;
static struct k_thread thread_data_motor2;
static k_tid_t tid_autohoming[2] = {0};

typedef enum tmc5041_registers_e
{
  REG_IDX_XACTUAL,
  REG_IDX_VSTART,
  REG_IDX_XTARGET,
  REG_IDX_SW_MODE,
  REG_IDX_RAMP_STAT,
  REG_IDX_COOLCONF,
  REG_IDX_DRV_STATUS,
  REG_IDX_COUNT
} tmc5041_registers_t;

const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][2] = {
    {0x21, 0x41}, // XACTUAL
    {0x23, 0x43}, // VSTART
    {0x2D, 0x4D}, // XTARGET
    {0x34, 0x54}, // SW_MODE
    {0x35, 0x55}, // RAMP_STAT
    {0x6D, 0x7D}, // COOLCONF
    {0x6F, 0x7F}, // DRV_STATUS
};

#define MOTOR_INIT_VMAX             80000
#define MOTOR_FS_VMAX               800000
#define MOTOR1_FULL_COURSE_STEPS    (256*600)

static struct spi_config spi_cfg = {
    .frequency = 1000000,
    .operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA,
    .cs = SPI_CS_CONTROL_PTR_DT(SPI_DEVICE, 2)
};
static const struct device *spi_bus_controller = DEVICE_DT_GET(SPI_CONTROLLER_NODE);

typedef struct
{
  int x0;           // step at x=0 (middle position)
  int x_current;
  uint32_t motor_state;
} motors_refs_t;

static motors_refs_t motors_refs[2] = {0};

static struct spi_buf rx;
static struct spi_buf_set rx_bufs = {
    .buffers = &rx,
    .count = 1
};

static struct spi_buf tx;
static struct spi_buf_set tx_bufs = {
    .buffers = &tx,
    .count = 1
};

const uint64_t motor_init_for_velocity_mode[2][11] = {
    {
        0x8000000008,
        0xEC000100C5, // 0x6C CHOPCONF
        0xB000011000, // IHOLD_IRUN reg, bytes from left to right: I_HOLD=0, I_RUN=16, IHOLDDELAY=1
        0xAC00002710,
        0x90000401C8, // PWMCONF
        0xB200061A80,
        0xB100000000
            + (MOTOR_INIT_VMAX
                / 2), // VCOOLTHRS: StallGuard enabled when motor reaches that velocity. Let's use ~VMAX/2
        0xA600001388,
        0xA700000000 + MOTOR_INIT_VMAX, // VMAX
        0xB400000000 | (1 << 10), // SW_MODE sg_stop
        0xA000000001
    },
    {
        0x8000000008,
        0xFC000100C5, // 0x6C CHOPCONF
        0xD000011000, // IHOLD_IRUN reg, bytes from left to right: I_HOLD=0, I_RUN=16, IHOLDDELAY=1
        0xCC00002710,
        0x98000401C8, // PWMCONF
        0xD200061A80,
        0xD100000000
            + (MOTOR_INIT_VMAX
                / 2), // VCOOLTHRS: StallGuard enabled when motor reaches that velocity. Let's use ~VMAX/2
        0xC600001388,
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX
        0xD400000000 | (1 << 10), // SW_MODE sg_stop
        0xC000000001
    }
};

const uint64_t position_mode_initial_phase[2][8] = {
    {
        0xA4000003E8, // A1 first acceleration
        0xA50000C350, // V1 Acceleration threshold, velocity V1
        0xA6000001F4, // Acceleration above V1
        0xA700000000 + MOTOR_INIT_VMAX, // VMAX
        0xA8000002BC, // DMAX Deceleration above V1
        0xAA00000578, // D1 Deceleration below V1
        0xAB0000000A, // VSTOP stop velocity
        0xA000000000, // RAMPMODE = 0 position move
    },
    {
        0xC4000003E8, // A1 first acceleration
        0xC50000C350, // V1 Acceleration threshold, velocity V1
        0xC6000001F4, // Acceleration above V1
        0xC700000000 + MOTOR_INIT_VMAX, // VMAX
        0xC8000002BC, // DMAX Deceleration above V1
        0xCA00000578, // D1 Deceleration below V1
        0xCB0000000A, // VSTOP stop velocity
        0xC000000000, // RAMPMODE = 0 position move
    }
};

const uint64_t position_mode_full_speed[2][8] = {
    {
        0xA400008000, // A1 first acceleration
        0xA500000000 + MOTOR_FS_VMAX * 3 / 4, // V1 Acceleration threshold, velocity V1
        0xA600001000, // Acceleration above V1
        0xA700000000 + MOTOR_FS_VMAX, // VMAX
        0xA800001000, // DMAX Deceleration above V1
        0xAA00008000, // D1 Deceleration below V1
        0xAB00000010, // VSTOP stop velocity
        0xA000000000, // RAMPMODE = 0 position move
    },
    {
        0xC400008000, // A1 first acceleration
        0xC500000000 + MOTOR_FS_VMAX * 3 / 4, // V1 Acceleration threshold, velocity V1
        0xC600001000, // Acceleration above V1
        0xC700000000 + MOTOR_FS_VMAX, // VMAX
        0xC800001000, // DMAX Deceleration above V1
        0xCA00008000, // D1 Deceleration below V1
        0xCB00000010, // VSTOP stop velocity
        0xC000000000, // RAMPMODE = 0 position move
    }
};

static void
motor_spi_send_commands(const struct device *spi_bus_controller, const uint64_t *cmds, size_t num_cmds)
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
motor_spi_write(const struct device *spi_bus_controller, uint8_t reg, uint32_t value)
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

    uint32_t read_value =
        (rx_buffer[1] << 24 |
            rx_buffer[2] << 16 |
            rx_buffer[3] << 8 |
            rx_buffer[4] << 0);

    return read_value;
}

static ret_code_t
motors_set_angle(int8_t d, uint8_t motor)
{
    __ASSERT(d <= 100 && d >= -100, "Accepted range is [-100;100]");
    __ASSERT(motor <= 1, "Motor number not handled");

    if (motors_refs[motor].motor_state) {
        return motors_refs[motor].motor_state;
    }

    LOG_INF("Setting motor %u to: %d", motor, d);

    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                    (uint32_t) motors_refs[motor].x0 + ((int32_t) d * (MOTOR1_FULL_COURSE_STEPS / 2) / 100));

    return RET_SUCCESS;
}

ret_code_t
motors_angle_horizontal(int8_t x)
{
    return motors_set_angle(x, 0);
}

ret_code_t
motors_angle_vertical(int8_t x)
{
    return motors_set_angle(x, 1);
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
    uint32_t motor = (uint32_t) p1;

    LOG_INF("Initializing motor %u", motor);

    // VSTART
    motor_spi_write(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_VSTART][motor], 0x0);

    // COOLCONF, set SGT to offset StallGuard value
    motor_spi_write(spi_bus_controller,
                    TMC5041_REGISTERS[REG_IDX_COOLCONF][motor],
                    (5 << 16) | (1 << 24) /* enable SG filter */);

    // start in one direction until stall is detected
    motor_spi_send_commands(spi_bus_controller,
                            motor_init_for_velocity_mode[motor],
                            ARRAY_SIZE(motor_init_for_velocity_mode[motor]));

    uint32_t status;
    int32_t x_first_end = 0;
    // timeout to detect first end
    // in unit of 250ms = 5 seconds
    // = maximum time to go from one side to the other during auto-homing + some delay
    uint32_t timeout = 16;

    while (1) {
        status = motor_spi_read(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_DRV_STATUS][motor]);
        LOG_DBG("Status %d 0x%08x", motor, status);

        // motor stall detection done by checking either:
        // - motor stopped by using sg_stop (status flag)
        // OR
        // - timeout==0 means the motor is blocked in end course (didn't move at all, preventing sg_stop from working)
        if (x_first_end == 0 && (status & (1 << 24) || --timeout == 0)) {
            if (timeout == 0) {
                LOG_WRN("Timeout while looking for first end on motor %d", motor);
            }

            // disable sg_stop
            motor_spi_write(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_SW_MODE][motor], 0x0);

            // read current position
            x_first_end = (int32_t) motor_spi_read(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
            LOG_INF("Motor %u stalled. First end pos: %d", motor, x_first_end);

            // clear events
            // the motor can be re-enabled by reading RAMP_STAT
            motor_spi_read(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // go to the other end using position mode
            motor_spi_send_commands(spi_bus_controller,
                                    position_mode_initial_phase[motor],
                                    ARRAY_SIZE(position_mode_initial_phase[motor]));
            // ready to move, let's go to the other end
            motor_spi_write(spi_bus_controller,
                            TMC5041_REGISTERS[REG_IDX_XTARGET][motor],
                            (uint32_t) (x_first_end - MOTOR1_FULL_COURSE_STEPS));

            // add longer delay before we continue
            // as we need to wait for the motor to move and removes its stall detection flag
            k_msleep(500);
            continue;
        }

        if (x_first_end != 0 && status & (1 << 24)) {
            LOG_WRN("Motor %u stalled when trying to reach other end", motor);
            err_code = RET_ERROR_INVALID_STATE;
            break;
        }

        if (x_first_end != 0 && status & (1 << 31)) {
            motor_spi_read(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_RAMP_STAT][motor]);

            // read current position
            int32_t x = (int32_t) motor_spi_read(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XACTUAL][motor]);
            LOG_INF("Motor %u reached other end, pos %d", motor, x);

            if (abs(x - (x_first_end - MOTOR1_FULL_COURSE_STEPS)) > 256) {
                LOG_INF("Didn't reached target: x=%d, should be ~ %d", x, (x_first_end - MOTOR1_FULL_COURSE_STEPS));

                err_code = RET_ERROR_INVALID_STATE;
                break;
            }

            motors_refs[motor].x0 = x + (MOTOR1_FULL_COURSE_STEPS / 2);

            LOG_INF("Motor %u, x0: %d", motor, motors_refs[motor].x0);

            // go to middle position
            motor_spi_write(spi_bus_controller, TMC5041_REGISTERS[REG_IDX_XTARGET][motor], motors_refs[motor].x0);

            motor_spi_send_commands(spi_bus_controller,
                                    position_mode_full_speed[motor],
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
motors_auto_homing(uint8_t motor)
{
    __ASSERT(motor <= 1, "Wrong motor number");

    if (tid_autohoming[motor] != NULL) {
        LOG_ERR("Motor %u auto-homing already in progress", motor);
        return RET_ERROR_FORBIDDEN;
    }

    if (motor == 0) {
        tid_autohoming[motor] =
            k_thread_create(&thread_data_motor1, stack_area_motor1_init, K_THREAD_STACK_SIZEOF(stack_area_motor1_init),
                            motors_auto_homing_thread, 0, NULL, NULL,
                            THREAD_PRIORITY_MOTORS_INIT, 0, K_NO_WAIT);
    } else {
        tid_autohoming[motor] =
            k_thread_create(&thread_data_motor2, stack_area_motor2_init, K_THREAD_STACK_SIZEOF(stack_area_motor2_init),
                            motors_auto_homing_thread, (void *) 1, NULL, NULL,
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

    motors_refs[0].motor_state = RET_ERROR_NOT_INITIALIZED;
    motors_refs[1].motor_state = RET_ERROR_NOT_INITIALIZED;

    motors_auto_homing(0);
    motors_auto_homing(1);

    return RET_SUCCESS;
}
