#include "uart_messaging.h"
#include <app_assert.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uart_messaging, CONFIG_UART_MESSAGING_LOG_LEVEL);

static const struct device *uart_dev =
    DEVICE_DT_GET(DT_PROP(DT_PATH(zephyr_user), jetson_serial));

#define RX_BUF_COUNT             2UL
#define UART_MESSAGE_HEADER_SIZE 4UL

struct uart_message_s {
    union {
        struct {
            uint16_t magic;
            uint16_t payload_size;
            uint8_t payload[CONFIG_ORB_LIB_UART_MAX_SIZE_BYTES -
                            UART_MESSAGE_HEADER_SIZE];
        } header_payload;
        uint8_t bytes[CONFIG_ORB_LIB_UART_MAX_SIZE_BYTES];
    } message;
};

static uint8_t uart_rx_buf[RX_BUF_COUNT][CONFIG_ORB_LIB_UART_MAX_SIZE_BYTES];
static struct uart_message_s payload;
static size_t wr_idx = 0;

static void (*incoming_message_handler)(void *msg);

static struct k_work uart_work;

static K_SEM_DEFINE(sem_msg, 1, 1);
static uart_message_t uart_message;

#define INVALID_INDEX 0xFFFFFFFF

static void
new_message(struct k_work *item)
{
    UNUSED_PARAMETER(item);

    CRITICAL_SECTION_ENTER(k);

    // if magic word, it means we have a protobuf message
    if (uart_message.bytes[0] == 0x8e && uart_message.bytes[1] == 0xad) {
        // remove magic word + length
        uart_message.size = *(uint16_t *)&uart_message.bytes[2];
        uart_message.bytes = &uart_message.bytes[UART_MESSAGE_HEADER_SIZE];

        LOG_INF("McuMessage through UART, size: %u", uart_message.size);

        incoming_message_handler(&uart_message);
        uart_message.size = 0;
    } else {
        LOG_WRN("received %uB that cannot be parsed: 0x%02x%02x",
                uart_message.size, uart_message.bytes[0],
                uart_message.bytes[1]);
    }

    CRITICAL_SECTION_EXIT(k);

    k_sem_give(&sem_msg);
}

static void
uart_receive_callback(const struct device *dev, struct uart_event *evt,
                      void *user_data)
{
    int ret;
    static uint8_t *next_buffer = uart_rx_buf[1];

    switch (evt->type) {
    case UART_RX_RDY:
        //        LOG_DBG("rdy, off %u, len %u, buf #%u", evt->data.rx.offset,
        //                evt->data.rx.len,
        //                evt->data.rx_buf.buf == uart_rx_buf[0] ? 0 : 1);

        if (evt->data.rx.len >= UART_MESSAGE_HEADER_SIZE &&
            evt->data.rx.buf[evt->data.rx.offset] == 0x8e &&
            evt->data.rx.buf[evt->data.rx.offset + 1] == 0xad) {
            // new message
            uint16_t payload_size =
                *(uint16_t *)&evt->data.rx.buf[evt->data.rx.offset + 2];

            if (payload_size <= evt->data.rx.len + UART_MESSAGE_HEADER_SIZE) {
                // we can use message directly from DMA without copy
                if (k_sem_take(&sem_msg, K_NO_WAIT) == 0) {
                    uart_message.bytes = &evt->data.rx.buf[evt->data.rx.offset];
                    uart_message.size = payload_size;
                    k_work_submit(&uart_work);
                } else {
                    k_sem_give(&sem_msg);
                }
            } else {
                // payload is overlapping 2 UART RX buffers
                // we need to copy message into buffer
                memcpy((void *)&payload, &evt->data.rx.buf[evt->data.rx.offset],
                       evt->data.rx.len);
                wr_idx = evt->data.rx.len;
            }
        } else if (wr_idx >= UART_MESSAGE_HEADER_SIZE &&
                   wr_idx + evt->data.rx.len >=
                       payload.message.header_payload.payload_size) {
            memcpy(&payload.message.bytes[wr_idx],
                   &evt->data.rx.buf[evt->data.rx.offset],
                   (wr_idx + evt->data.rx.len) >
                           (payload.message.header_payload.payload_size +
                            UART_MESSAGE_HEADER_SIZE)
                       ? payload.message.header_payload.payload_size - wr_idx -
                             UART_MESSAGE_HEADER_SIZE
                       : evt->data.rx.len);

            if (k_sem_take(&sem_msg, K_NO_WAIT) == 0) {
                uart_message.bytes = payload.message.bytes;
                uart_message.size = wr_idx;
                k_work_submit(&uart_work);
                wr_idx = 0;
            } else {
                k_sem_give(&sem_msg);
            }
        } else {
            memcpy(&payload.message.bytes[wr_idx],
                   &evt->data.rx.buf[evt->data.rx.offset], evt->data.rx.len);
            wr_idx += evt->data.rx.len;

            if (wr_idx > UART_MESSAGE_HEADER_SIZE &&
                wr_idx >= payload.message.header_payload.payload_size +
                              UART_MESSAGE_HEADER_SIZE) {
                if (k_sem_take(&sem_msg, K_NO_WAIT) == 0) {
                    uart_message.bytes = payload.message.bytes;
                    uart_message.size = wr_idx;
                    k_work_submit(&uart_work);
                    wr_idx = 0;
                } else {
                    k_sem_give(&sem_msg);
                }
            }
        }
        break;
    case UART_RX_BUF_RELEASED:
        next_buffer = evt->data.rx_buf.buf;
        break;
    case UART_RX_BUF_REQUEST:
        ret = uart_rx_buf_rsp(dev, next_buffer,
                              CONFIG_ORB_LIB_UART_MAX_SIZE_BYTES);
        ASSERT_SOFT(ret);
        break;
    default:
        LOG_ERR("Unhandled event %d", evt->type);
        break;
    }

    UNUSED_PARAMETER(user_data);
}

int
uart_messaging_init(void (*in_handler)(void *msg))
{
    if (in_handler == NULL) {
        return RET_ERROR_INVALID_PARAM;
    } else {
        incoming_message_handler = in_handler;
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
    ret = uart_rx_enable(uart_dev, uart_rx_buf[0], sizeof uart_rx_buf[0], 0);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    k_work_init(&uart_work, new_message);

    return RET_SUCCESS;
}
