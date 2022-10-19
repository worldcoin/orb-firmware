#include "gnss.h"
#include "mcu_messaging.pb.h"
#include "utils.h"
#include <app_assert.h>
#include <app_config.h>
#include <assert.h>
#include <device.h>
#include <drivers/uart.h>
#include <pubsub/pubsub.h>
#include <stdlib.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(gnss, CONFIG_GNSS_LOG_LEVEL);

#define NMEA_MAX_SIZE 82 // Includes starting '$' and '\r' '\n'

#define GNSS_NODE DT_PATH(zephyr_user)
#define GNSS_CTRL DT_PROP(GNSS_NODE, gnss)

static const struct device *uart_dev = DEVICE_DT_GET(GNSS_CTRL);

static K_THREAD_STACK_DEFINE(stack_area, THREAD_STACK_SIZE_GNSS);
static struct k_thread gnss_thread_data;

static uint8_t uart_buffer1[NMEA_MAX_SIZE * 2];
static uint8_t uart_buffer2[sizeof uart_buffer1];

K_MSGQ_DEFINE(uart_chars_q, 1, (sizeof uart_buffer1) * 2, 1);

static void
uart_receive_callback(const struct device *dev, struct uart_event *evt,
                      void *user_data)
{
    UNUSED_PARAMETER(user_data);

    static uint8_t *next_buffer = uart_buffer2;
    int ret;

    switch (evt->type) {
    case UART_RX_BUF_REQUEST:
        ret = uart_rx_buf_rsp(dev, next_buffer, sizeof uart_buffer1);
        ASSERT_SOFT(ret);
        break;
    case UART_RX_RDY:
        for (size_t i = 0; i < evt->data.rx.len && i < sizeof uart_buffer1;
             ++i) {
            ret = k_msgq_put(&uart_chars_q,
                             evt->data.rx.buf + evt->data.rx.offset + i,
                             K_NO_WAIT);
            if (ret) {
                LOG_ERR("Not consuming fast enough!");
            }
        }
        break;
    case UART_RX_BUF_RELEASED:
        next_buffer = evt->data.rx_buf.buf;
        break;
    case UART_TX_DONE:
    case UART_TX_ABORTED:
        break;
    default:
        LOG_ERR("Unhandled event %d", evt->type);
        break;
    };
}

static char
get_char(void)
{
    char ch;
    k_msgq_get(&uart_chars_q, &ch, K_FOREVER);
    return ch;
}

// Assumes msg len is at least (NMEA_MAX_SIZE + 1) and that '$' has just been
// returned from get_char()
static ret_code_t
parse_nmea(char *msg)
{
    char ch;
    size_t index = 0;
    uint8_t calculated_checksum = 0;

    msg[index++] = '$';

    // look for checksum delimiter
    ch = get_char();
    while (index < NMEA_MAX_SIZE && ch != '*') {
        msg[index++] = ch;
        calculated_checksum ^= ch;
        ch = get_char();
    }

    if (index == NMEA_MAX_SIZE) {
        LOG_HEXDUMP_ERR(msg, NMEA_MAX_SIZE, "Invalid NMEA 0183 msg");
        return RET_ERROR_NOT_FOUND;
    }

    msg[index++] = ch;

    // Retrieve the checksum
    char checksum[3];

    checksum[0] = get_char();
    checksum[1] = get_char();
    checksum[2] = '\0';

    // a 2 byte hex number cannot overflow a uint8_t
    char *endptr;
    uint8_t read_checksum = strtoul(checksum, &endptr, 16);

    if (*checksum == '-' || *endptr != '\0') {
        LOG_ERR("Checksum NaN: %02x%02x", checksum[0], checksum[1]);
        return RET_ERROR_NOT_FOUND;
    }

    msg[index++] = checksum[0];
    msg[index++] = checksum[1];

    if (calculated_checksum != read_checksum) {
        LOG_ERR(
            "We calculated a checksum of 0x%02x, but got a checksum of 0x%02x",
            calculated_checksum, read_checksum);
        return RET_ERROR_NOT_FOUND;
    }

    if ((ch = get_char()) != '\r') {
        LOG_ERR("Expected a terminating '\\r' but got '0x%02x'", ch);
        return RET_ERROR_NOT_FOUND;
    }

    msg[index++] = ch;

    if ((ch = get_char()) != '\n') {
        LOG_ERR("Expected a terminating '\\n' but got '0x%02x'", ch);
        return RET_ERROR_NOT_FOUND;
    }

    msg[index++] = ch;
    msg[index] = '\0';

    return RET_SUCCESS;
}

static void
send_nmea_message(const char *nmea_str)
{
    static uint32_t counter = 0;
    static GNSSDataPartial msg;
    size_t len = strlen(nmea_str);
    size_t min = MIN(len, sizeof msg.nmea_part - 1);
    memcpy(msg.nmea_part, nmea_str, min);
    msg.nmea_part[min] = '\0';
    msg.counter = counter++;
    publish_new(&msg, sizeof(msg), McuToJetson_gnss_partial_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    strncpy(msg.nmea_part, nmea_str + min, sizeof msg.nmea_part - 1);
    msg.counter = counter++;
    publish_new(&msg, sizeof(msg), McuToJetson_gnss_partial_tag,
                CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
}

static void
gnss_thread_entry_point()
{
    ret_code_t ret;
    char ch;
    static char msg[NMEA_MAX_SIZE + 1];

    static_assert(sizeof msg <= STRUCT_MEMBER_ARRAY_SIZE(GNSSData, nmea),
                  "msg must be small enought to fit into protobuf message");

    for (;;) {
        ch = get_char();

        if (ch == '$') { // NMEA 0183 message start
            ret = parse_nmea(msg);
            if (ret == RET_SUCCESS) {
                // prevent new line characters from being printed by using
                // length `strlen(msg) - 2`
                LOG_DBG("Got NMEA message: %.*s", strlen(msg) - 2, msg);

                send_nmea_message(msg);
            }
        } // else ignore until we see something we recognize
    }
}

ret_code_t
gnss_init(void)
{
    int ret;

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("GNSS device not ready!");
        return RET_ERROR_INVALID_STATE;
    }

    ret = uart_callback_set(uart_dev, uart_receive_callback, NULL);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    k_thread_create(&gnss_thread_data, stack_area,
                    K_THREAD_STACK_SIZEOF(stack_area), gnss_thread_entry_point,
                    NULL, NULL, NULL, THREAD_PRIORITY_GNSS, 0, K_NO_WAIT);
    k_thread_name_set(&gnss_thread_data, "gnss");

    ret = uart_rx_enable(uart_dev, uart_buffer1, sizeof uart_buffer1, 1000);
    if (ret) {
        ASSERT_SOFT(ret);
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}
