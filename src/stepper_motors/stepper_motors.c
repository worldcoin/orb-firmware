#include "stepper_motors.h"
#include <device.h>
#include <zephyr.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(stepper_motors);

#define SPI_DEVICE DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ 0
#define WRITE (1 << 7)

#define TMC5041_IC_VERSION 0x10

#define REG_INPUT       0x04
#define REG_VCOOLTHRS   0x31
#define REG_COOLCONF    0x6D
#define REG_DRV_STATUS  0x6F

static struct spi_config spi_cfg = {
    .frequency = 1000000,
    .operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA,
    .cs = SPI_CS_CONTROL_PTR_DT(SPI_DEVICE, 2)
};

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
    0xB000011000, // bytes from left to right: I_HOLD=0, I_RUN=16, IHOLDDELAY=1
    0xAC00002710,
    0x90000401C8,
    0xB200061A80,
    0xB100007500, // VCOOLTHRS
    0xA600001388,
    0xA700007530,
    0xA000000001
};

const uint64_t init_for_position_mode[] = {
    0xA4000003E8,
    0xA50000C350,
    0xA6000001F4,
    0xA7000304D0,
    0xA8000002BC,
    0xAA00000578,
    0xAB0000000A,
    0xA000000001
};

void spi_send_commands(const struct device *spi_bus_controller, const uint64_t *cmds, size_t num_cmds)
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

void motor_spi_write(const struct device *spi_bus_controller, uint8_t reg, uint32_t value)
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

uint32_t motor_spi_read(const struct device *spi_bus_controller, uint8_t reg)
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

ret_code_t init_stepper_motors(void)
{
    const struct device *spi_bus_controller = DEVICE_DT_GET(SPI_CONTROLLER_NODE);
    uint32_t read_value = 0;

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

    spi_send_commands(spi_bus_controller,
                      init_for_position_mode,
                      sizeof(init_for_position_mode) / sizeof(init_for_position_mode[0]));

    // COOLCONF, set SGT to offset StallGuard value
    motor_spi_write(spi_bus_controller, REG_COOLCONF, (5 << 16));

    k_msleep(100);

    // start in one direction for testing
    spi_send_commands(spi_bus_controller,
                      init_for_velocity_mode,
                      ARRAY_SIZE(init_for_velocity_mode));

    uint8_t direction = 0;
    uint32_t status;
    while (1) {
        status = motor_spi_read(spi_bus_controller, REG_DRV_STATUS);
        LOG_INF("Status 0x%08x", status);

        uint8_t current_level = ((status >> 16) & 0x1F);
        LOG_INF("Current level: %u", current_level);

        // if motor stalled, invert direction
        if (status & (1 << 24)) {
            LOG_WRN("StallGuard flag");

            motor_spi_write(spi_bus_controller, 0x20, 1 << direction);
            direction = 1 - direction;
        }

        k_msleep(250);
    }
}
