#include "stepper_motors.h"
#include <device.h>
#include <zephyr.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>

#include <logging/log.h>
#include <stdlib.h>
LOG_MODULE_REGISTER(stepper_motors);

#define SPI_DEVICE DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ 0
#define WRITE (1 << 7)

#define TMC5041_IC_VERSION 0x10

#define REG_INPUT       0x04
#define REG_COOLCONF    0x6D
#define REG_DRV_STATUS  0x6F

#define MOTOR_INIT_VMAX             60000
#define MOTOR1_FULL_COURSE_STEPS    (256*600)

static struct spi_config spi_cfg = {
    .frequency = 1000000,
    .operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA,
    .cs = SPI_CS_CONTROL_PTR_DT(SPI_DEVICE, 2)
};

typedef struct {
  int x0;           // step at x=0 (middle position)
  int x_current;
  int y0;           // step at y=0 (middle position)
  int y_current;
} motors_refs_t;

static motors_refs_t motors_refs = {0};

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

const uint64_t init_for_velocity_mode[] = {
    0x8000000008,
    0xEC000100C5,
    0xB000011000, // IHOLD_IRUN reg, bytes from left to right: I_HOLD=0, I_RUN=16, IHOLDDELAY=1
    0xAC00002710,
    0x90000401C8,
    0xB200061A80,
    0xB100000000 + (MOTOR_INIT_VMAX / 2), // VCOOLTHRS: StallGuard enabled when motor reaches that velocity. Let's use ~VMAX/2
    0xA600001388,
    0xA700000000 + MOTOR_INIT_VMAX, // VMAX
    0xB400000000 | (1 << 10), // SW_MODE sg_stop
    0xA000000001
};

const uint64_t init_for_position_mode[] = {
    0xA4000003E8, // A1 first acceleration
    0xA50000C350, // V1 Acceleration threshold, velocity V1
    0xA6000001F4, // Acceleration above V1
    0xA700000000 + MOTOR_INIT_VMAX, // VMAX
    0xA8000002BC, // DMAX Deceleration above V1
    0xAA00000578, // D1 Deceleration below V1
    0xAB0000000A, // VSTOP stop velocity
    0xA000000000, // RAMPMODE = 0 position move
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

ret_code_t
motors_init(void)
{
    const struct device *spi_bus_controller = DEVICE_DT_GET(SPI_CONTROLLER_NODE);
    uint32_t read_value;

    if (!device_is_ready(spi_bus_controller)) {
        LOG_ERR("motion controller SPI device not ready");
        return RET_ERROR_BUSY;
    } else {
        LOG_INF("Motion controller SPI ready");
    }

    read_value = motor_spi_read(spi_bus_controller, 0x00);
    LOG_INF("GCONF: 0x%08x", read_value);
    k_msleep(10);

    read_value = motor_spi_read(spi_bus_controller, REG_INPUT);
    LOG_INF("Input: 0x%08x", read_value);
    uint8_t ic_version = (read_value >> 24 & 0xFF);

    if (ic_version != TMC5041_IC_VERSION) {
        LOG_ERR("Error reading TMC5041");
        return RET_ERROR_INVALID_STATE;
    }

    // VSTART
    motor_spi_write(spi_bus_controller, 0x23, 0x0F);
    k_msleep(100);

    // COOLCONF, set SGT to offset StallGuard value
    motor_spi_write(spi_bus_controller, REG_COOLCONF, (5 << 16) | (1 << 24) /* enable SG filter */);

    k_msleep(100);

    // start in one direction for testing
    motor_spi_send_commands(spi_bus_controller,
                            init_for_velocity_mode,
                            ARRAY_SIZE(init_for_velocity_mode));

    uint32_t status;
    int32_t x_first_end = 0;

    while (1) {
        status = motor_spi_read(spi_bus_controller, REG_DRV_STATUS);
        LOG_DBG("Status 0x%08x", status);

        // motor stalled, stopped by using sg_stop

        if (x_first_end == 0 && status & (1 << 24)) {
            LOG_WRN("Motor is stalled");

            // disable sg_stop
            motor_spi_write(spi_bus_controller, 0xB4, 0x0);

            // read current position
            x_first_end = (int32_t) motor_spi_read(spi_bus_controller, 0x21);
            LOG_INF("XACTUAL 1: %d", x_first_end);

            // clear events
            // the motor can be re-enabled by reading RAMP_STAT
            uint32_t stalled_status = motor_spi_read(spi_bus_controller, 0x35);
            LOG_INF("RAMP_STAT: 0x%08x", stalled_status);

            // go to the other end using position mode
            motor_spi_send_commands(spi_bus_controller,
                                    init_for_position_mode,
                                    sizeof(init_for_position_mode) / sizeof(init_for_position_mode[0]));

            // ready to move, let's go to the other end
            motor_spi_write(spi_bus_controller, 0x2D, (uint32_t) (x_first_end - MOTOR1_FULL_COURSE_STEPS));

            k_msleep(500);

            continue;
        }

        if (x_first_end != 0 && status & (1 << 24)) {
            LOG_WRN("Motor stalled when trying to reach other end");
            return RET_ERROR_INVALID_STATE;
        }

        if (x_first_end != 0 && status & (1 << 31)) {
            motor_spi_read(spi_bus_controller, 0x35);

            LOG_INF("Reached other end");

            // read current position
            int32_t x = (int32_t) motor_spi_read(spi_bus_controller, 0x21);
            LOG_INF("XACTUAL 2: %d", x);

            if (abs(x - (x_first_end - MOTOR1_FULL_COURSE_STEPS)) > 256)
            {
                LOG_INF("Didn't reached target");

                return RET_ERROR_INVALID_STATE;
            }

            motors_refs.x0 = x + (MOTOR1_FULL_COURSE_STEPS/2);

            LOG_INF("X0=%d", motors_refs.x0);

            // go to middle position
            motor_spi_write(spi_bus_controller, 0x2D, motors_refs.x0);

            return RET_SUCCESS;
        }

        k_msleep(250);
    }
}
