#include <device.h>
#include <zephyr.h>
#include <drivers/spi.h>

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
    .operation = SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPOL,
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

    tx_buffer[0] = READ | REG_INPUT;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;

    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    //k_msleep(10000);

    int ret = spi_transceive(spi_bus_controller, &spi_cfg, &tx_bufs, &rx_bufs);
    //int ret = spi_write(spi_bus_controller, &spi_cfg, &tx_bufs);

    LOG_INF("ret is %d", ret);

    LOG_INF("Response: ");

    for (int i = 0; i < sizeof rx_buffer; ++i) {
        LOG_INF("byte [%d]: 0x%02x", i, rx_buffer[0]);
    }

    return 0;
}
