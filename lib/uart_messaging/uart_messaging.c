#include "uart_messaging.h"
#include <app_assert.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

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
const uint16_t HEADER_MAGIC_U16 = 0xad8e; // uint16_t in little endian

static uint8_t
    __aligned(1) uart_rx_ring_buf[CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES];
static size_t read_index = SIZE_MAX;
static size_t write_index = SIZE_MAX;

// statically verify that buffer size is a power of 2
BUILD_ASSERT(CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES &&
                 (CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES &
                  (CONFIG_ORB_LIB_UART_RX_BUF_SIZE_BYTES - 1)) == 0,
             "must be power of 2");

#define UART_RX_RING_BUFFER_USED_BYTES(start, end)                             \
    ((end - start) & (sizeof(uart_rx_ring_buf) - 1))

static void (*incoming_message_handler)(void *msg);

// thread used to process queued ready-to-process messages
static K_THREAD_STACK_DEFINE(rx_thread_stack,
                             CONFIG_ORB_LIB_THREAD_STACK_SIZE_UART_RX);
static struct k_thread rx_thread_data = {0};
K_MSGQ_DEFINE(uart_recv_queue, sizeof(uart_message_t), 6, 4);

_Noreturn static void
rx_thread()
{
    uart_message_t message;

    while (1) {
        k_msgq_get(&uart_recv_queue, &message, K_FOREVER);

        incoming_message_handler(&message);
    }
}

/// Handle new UART bytes received on DMA interrupts
/// The callback detects the new messages by parsing the header and pushes
/// memory pointers (see \struct uart_message_t) to the payload for
/// further processing in a separate thread. See \fn rx_thread.
///
/// ⚠️ ISR context, keep it short
/// \param dev unused
/// \param evt rx data: buffer address, offset, length
/// \param user_data unused
static void
uart_event_callback(const struct device *dev, struct uart_event *evt,
                    void *user_data)
{
    UNUSED_PARAMETER(dev);
    UNUSED_PARAMETER(user_data);

    int ret;
    uart_message_t message = {
        uart_rx_ring_buf,
        sizeof(uart_rx_ring_buf),
        0,
        0,
    };

    switch (evt->type) {
    case UART_RX_RDY: {
        // initialize value
        if (read_index == SIZE_MAX && write_index == SIZE_MAX) {
            read_index = evt->data.rx.offset;
            write_index = evt->data.rx.offset;
        }

        write_index =
            (write_index + evt->data.rx.len) % sizeof(uart_rx_ring_buf);

        // check new fully-received messages
        while (read_index != write_index) {
            /// even if we didn't receive the header entirely, try to read both
            /// the header and the payload size where it should be located
            /// `header_magic` and `payload_size` must be used only if the bytes
            /// are all received by checking \def UART_RX_RING_BUFFER_USED_BYTES
            /// before using any of these
            uint16_t payload_size =
                (uint16_t)((uart_rx_ring_buf[(read_index + 3) %
                                             sizeof(uart_rx_ring_buf)]
                            << 8) +
                           uart_rx_ring_buf[(read_index + 2) %
                                            sizeof(uart_rx_ring_buf)]);
            uint16_t header_magic =
                (uint16_t)((uart_rx_ring_buf[(read_index + 1) %
                                             sizeof(uart_rx_ring_buf)]
                            << 8) +
                           uart_rx_ring_buf[read_index]);

            if (UART_RX_RING_BUFFER_USED_BYTES(read_index, write_index) >
                    UART_MESSAGE_HEADER_SIZE &&
                header_magic == HEADER_MAGIC_U16 &&
                UART_RX_RING_BUFFER_USED_BYTES(read_index, write_index) >=
                    (payload_size + UART_MESSAGE_HEADER_SIZE)) {
                // entire message received

                // remove header to get index to payload
                message.start_idx = (read_index + UART_MESSAGE_HEADER_SIZE) %
                                    sizeof(uart_rx_ring_buf);
                message.length = payload_size;

                ret = k_msgq_put(&uart_recv_queue, &message, K_NO_WAIT);
                if (ret) {
                    LOG_ERR("rx queue err %d", ret);
                }

                read_index =
                    (read_index + message.length + UART_MESSAGE_HEADER_SIZE) %
                    sizeof(uart_rx_ring_buf);
            } else if ((UART_RX_RING_BUFFER_USED_BYTES(read_index,
                                                       write_index) <
                        UART_MESSAGE_HEADER_SIZE) ||
                       ((UART_RX_RING_BUFFER_USED_BYTES(read_index,
                                                        write_index) >=
                         UART_MESSAGE_HEADER_SIZE) &&
                        header_magic == HEADER_MAGIC_U16 &&
                        (UART_RX_RING_BUFFER_USED_BYTES(read_index,
                                                        write_index) <
                         (payload_size + UART_MESSAGE_HEADER_SIZE)))) {
                // more bytes needed: no header or payload not entirely received

                return;
            } else {
                // header not detected, move into received bytes to find the
                // next UART message
                read_index = (read_index + 1) % sizeof(uart_rx_ring_buf);
            }
        }
    } break;
    case UART_RX_BUF_RELEASED:
    case UART_RX_BUF_REQUEST:
        break;
    default:
        LOG_ERR("Unhandled event %d", evt->type);
        break;
    }
}

#ifdef CONFIG_PM
int
uart_messaging_suspend(void)
{
    int ret = RET_SUCCESS;

    if (pm_device_is_busy(uart_dev)) {
        ret = uart_rx_disable(uart_dev);
        if (ret) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INVALID_STATE;
        }

        pm_device_busy_clear(uart_dev);
    }

    return ret;
}

int
uart_messaging_resume(void)
{
    int ret = RET_SUCCESS;

    if (!pm_device_is_busy(uart_dev)) {
        // no timeout, UART_RX_RDY as soon as uart reaches idle state
        ret = uart_rx_enable(uart_dev, uart_rx_ring_buf,
                             sizeof uart_rx_ring_buf, 0);
        if (ret) {
            ASSERT_SOFT(ret);
            return RET_ERROR_INVALID_STATE;
        }

        pm_device_busy_set(uart_dev);
    }

    return ret;
}
#endif

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

    int ret = uart_callback_set(uart_dev, uart_event_callback, NULL);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }

#ifndef CONFIG_PM
    ret =
        uart_rx_enable(uart_dev, uart_rx_ring_buf, sizeof uart_rx_ring_buf, 0);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INTERNAL;
    }
#endif

    k_tid_t tid = k_thread_create(
        &rx_thread_data, rx_thread_stack,
        K_THREAD_STACK_SIZEOF(rx_thread_stack), rx_thread, NULL, NULL, NULL,
        CONFIG_ORB_LIB_THREAD_PRIORITY_UART_RX, 0, K_NO_WAIT);
    k_thread_name_set(tid, "uart_rx");

    return RET_SUCCESS;
}
