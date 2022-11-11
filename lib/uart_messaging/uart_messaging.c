#include "uart_messaging.h"
#include <app_assert.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_messaging, CONFIG_UART_MESSAGING_LOG_LEVEL);

static const struct device *uart_dev =
    DEVICE_DT_GET_OR_NULL(DT_PROP(DT_PATH(zephyr_user), jetson_serial));

#if !DT_NODE_HAS_STATUS(DT_PROP(DT_PATH(zephyr_user), jetson_serial), okay)
#warning UART device not enabled, you can deselect CONFIG_ORB_LIB_UART_MESSAGING
#endif

/// Header is comprised of "magic" characters to mark the beginning of the
/// message and the message length on 2 bytes (uint16_t, Little Endian)
/// | 0x8E | 0xAD | len[0] | len[1] |
#define UART_MESSAGE_HEADER_SIZE 4UL
const uint8_t HEADER_MAGIC[2] = {0x8e, 0xad};

// twice the size defined in `CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES` is used in
// `uart_rx_buf` to continuously receive data using the circular DMA buffer
// when a message is chunked into two buffer parts, `bytes` is used to
// reconstitute it before decoding.
// `wr_idx` is the pointer into `bytes`
static uint8_t uart_rx_buf[CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES * 2];
static uint8_t bytes[CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES];
static size_t wr_idx = 0;

static void (*incoming_message_handler)(void *msg);

static struct k_work uart_work;

static K_SEM_DEFINE(sem_msg, 1, 1);
static uart_message_t uart_message;

static void
new_message(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    incoming_message_handler(&uart_message);

    k_sem_give(&sem_msg);
}

static void __attribute__((optimize("Ofast")))
uart_receive_callback(const struct device *dev, struct uart_event *evt,
                      void *user_data)
{
    UNUSED_PARAMETER(dev);
    UNUSED_PARAMETER(user_data);

    uint8_t *msg_ptr;

    switch (evt->type) {
    case UART_RX_RDY:
        if (wr_idx + evt->data.rx.len > sizeof(bytes)) {
            LOG_ERR("Overflow rdy: l %u\to %u\twr_idx %u", evt->data.rx.len,
                    evt->data.rx.offset, wr_idx);
            wr_idx = 0;
        }

        if (evt->data.rx.len > UART_MESSAGE_HEADER_SIZE &&
            evt->data.rx.buf[evt->data.rx.offset] == HEADER_MAGIC[0] &&
            evt->data.rx.buf[evt->data.rx.offset + 1] == HEADER_MAGIC[1] &&
            *(uint16_t *)&evt->data.rx.buf[evt->data.rx.offset + 2] +
                    UART_MESSAGE_HEADER_SIZE <=
                evt->data.rx.len) {
            // full message in one chunk
            wr_idx = evt->data.rx.len;
            msg_ptr = &evt->data.rx.buf[evt->data.rx.offset];
        } else {
            // chunked message to reconstitute

            // detect new message
            if (evt->data.rx.len > 2 &&
                evt->data.rx.buf[evt->data.rx.offset] == HEADER_MAGIC[0] &&
                evt->data.rx.buf[evt->data.rx.offset + 1] == HEADER_MAGIC[1]) {
                wr_idx = 0;
            }

            // copy into temporary buffer
            memcpy(&bytes[wr_idx], &evt->data.rx.buf[evt->data.rx.offset],
                   evt->data.rx.len);
            wr_idx += evt->data.rx.len;

            msg_ptr = bytes;
        }

        if (wr_idx > UART_MESSAGE_HEADER_SIZE &&
            msg_ptr[0] == HEADER_MAGIC[0] && msg_ptr[1] == HEADER_MAGIC[1] &&
            *(uint16_t *)&msg_ptr[2] + UART_MESSAGE_HEADER_SIZE <= wr_idx) {
            if (k_sem_take(&sem_msg, K_NO_WAIT) == 0) {
                // remove header and send payload to handler as it is
                uart_message.bytes = &msg_ptr[UART_MESSAGE_HEADER_SIZE];
                uart_message.size = wr_idx - UART_MESSAGE_HEADER_SIZE;
                k_work_submit(&uart_work);
            }
            wr_idx = 0;
        }
        break;
    case UART_RX_BUF_RELEASED:
    case UART_RX_BUF_REQUEST:
        break;
    default:
        LOG_ERR("Unhandled event %d", evt->type);
        break;
    }
}

int
uart_messaging_init(void (*in_handler)(void *msg))
{
    if (in_handler == NULL) {
        return RET_ERROR_INVALID_PARAM;
    } else {
        incoming_message_handler = in_handler;
    }

    if (uart_dev == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready!");
        return RET_ERROR_INVALID_STATE;
    }

    int ret = uart_callback_set(uart_dev, uart_receive_callback, NULL);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    // no timeout, UART_RX_RDY as soon as uart reaches idle state
    ret = uart_rx_enable(uart_dev, uart_rx_buf, sizeof uart_rx_buf, 0);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    k_work_init(&uart_work, new_message);

    return RET_SUCCESS;
}
