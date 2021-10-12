#include <device.h>
#include <zephyr.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>


#include <logging/log.h>
LOG_MODULE_REGISTER(stepper_motors);

#include "stepper_motors.h"

#define SPI_DEVICE DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ 0
#define WRITE (1 << 7)

#define REG_INPUT 0x04

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
    0xB000011F05,
    0xAC00002710,
    0x90000401C8,
    0xB200061A80,
    0xB100007530,
    0xA600001388,
    0xA700004E20,
    0xA000000002
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

int init_stepper_motors(void)
{
    uint8_t tx_buffer[5];
    uint8_t rx_buffer[5];
    const struct device *spi_bus_controller = DEVICE_DT_GET(SPI_CONTROLLER_NODE);

    if (!device_is_ready(spi_bus_controller)) {
        LOG_ERR("motion controller SPI device not ready");
        return 1;
    } else {
        LOG_INF("Motion controler SPI ready");
    }

    memset(tx_buffer, 0, sizeof tx_buffer);
    tx_buffer[0] = READ | REG_INPUT;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;

    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    int ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);

    spi_send_commands(spi_bus_controller, init_for_velocity_mode, ARRAY_SIZE(init_for_velocity_mode));

    tx.buf = tx_buffer;
    memset(tx_buffer, 0, sizeof tx_buffer);

/*

    tx_buffer[0] = 0xed;
    //tx_buffer[2] = 0x7f;
    tx_buffer[2] = 0x40;

    for (int i = 0; i < 5; ++i) {
        LOG_INF("tx_buffer[%d] = 0x%02x", i, tx_buffer[i]);
    }
    spi_write(spi_bus_controller, &spi_cfg, &tx_bufs);
*/
    tx.buf = tx_buffer;
    memset(tx_buffer, 0, sizeof tx_buffer);
    tx_buffer[0] = 0x6f;

    uint16_t stall_val;
    while (1) {
        spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
        stall_val = ((rx_buffer[3] & 1) << 8) | rx_buffer[4];

        for (int i = 0; i < 5; ++i) {
            LOG_INF("rx_buffer[%d] = 0x%02x", i, rx_buffer[i]);
        }

        LOG_INF("stall value: %u", stall_val);

        spi_read(spi_bus_controller, &spi_cfg, &rx_bufs);
        stall_val = ((rx_buffer[3] & 1) << 8) | rx_buffer[4];

        for (int i = 0; i < 5; ++i) {
            LOG_INF("rx_buffer[%d] = 0x%02x", i, rx_buffer[i]);
        }

        LOG_INF("stall value: %u", stall_val);

        k_msleep(100);
    }
    return 0;
}
