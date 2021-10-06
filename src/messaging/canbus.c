//
// Created by Cyril on 30/09/2021.
//

#include "canbus.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(canbus);

#include "mcu_messaging.pb.h"
#include "messaging.h"
#include <device.h>
#include <zephyr.h>
#include <canbus/isotp.h>
#include <pb.h>
#include <pb_decode.h>

#include <app_config.h>

#define RX_ADDR         CONFIG_CAN_ADDRESS_MCU     // MCU
#define TX_ADDR         CONFIG_CAN_ADDRESS_JETSON  // Jetson address

#define RX_BUF_SIZE     256

const struct device *can_dev;
const struct isotp_fc_opts flow_control_opts = {.bs = 8, .stmin = 0};

#define RX_THREAD_STACK_SIZE    2048
#define RX_THREAD_PRIORITY      5

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);
static struct k_thread rx_thread_data;

const struct isotp_msg_id rx_addr = {
    .std_id = RX_ADDR,
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0
};
const struct isotp_msg_id tx_addr = {
    .std_id = TX_ADDR,
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0
};

_Noreturn static void
rx_thread(void *arg1, void *arg2, void *arg3)
{
    struct net_buf *buf = NULL;
    int ret, rem_len;
    struct isotp_recv_ctx recv_ctx;
    uint8_t rx_buffer[RX_BUF_SIZE] = {0};
    size_t wr_idx = 0;

    ret = isotp_bind(&recv_ctx, can_dev,
                     &tx_addr, &rx_addr,
                     &flow_control_opts, K_FOREVER);
    __ASSERT(ret == ISOTP_N_OK, "Failed to bind to rx ID %d [%d]", rx_addr.std_id, ret);

    while (1) {
        wr_idx = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));

        // enter receiving loop
        // we will not exit until all the bytes are received or timeout
        do {
            // get new block (BS)
            rem_len = isotp_recv_net(&recv_ctx, &buf,
                                     K_MSEC(2000));
            if (rem_len < ISOTP_N_OK) {
                LOG_DBG("Receiving error [%d]", rem_len);
                break;
            }

            if (wr_idx + buf->len < sizeof(rx_buffer)) {
                memcpy(&rx_buffer[wr_idx], buf->data, buf->len);
                wr_idx += buf->len;
            } else {
                // todo push data and reset rx buf
            }

            if (buf != NULL) {
                net_buf_unref(buf);
            }
        } while (rem_len);

        if (rem_len == ISOTP_N_OK) {
            pb_istream_t stream = pb_istream_from_buffer(rx_buffer, wr_idx);
            McuMessage data = {0};

            bool decoded = pb_decode(&stream, McuMessage_fields, &data);
            if (decoded) {
                messaging_push_rx(&data);
            } else {
                LOG_ERR("Error parsing data, discarding");
            }

        } else {
            LOG_DBG("Data not received: %d", rem_len);
        }
    }
}

ret_code_t
canbus_send(const char *data, size_t len, void (*tx_complete_cb)(int, void *))
{
    static struct isotp_send_ctx send_ctx;
    memset(&send_ctx, 0, sizeof(send_ctx));

    int ret = isotp_send(&send_ctx, can_dev,
                         data, len,
                         &tx_addr, &rx_addr,
                         tx_complete_cb, NULL);
    if (ret != ISOTP_N_OK) {
        LOG_ERR("Error while sending data to ID %d [%d]",
                tx_addr.std_id, ret);

        return RET_ERROR_INTERNAL;
    }

    return RET_SUCCESS;
}

ret_code_t
canbus_init(void)
{
    can_dev = device_get_binding(DT_CHOSEN_ZEPHYR_CAN_PRIMARY_LABEL);
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    k_tid_t tid = k_thread_create(&rx_thread_data, rx_thread_stack,
                                  K_THREAD_STACK_SIZEOF(rx_thread_stack),
                                  rx_thread, NULL, NULL, NULL,
                                  RX_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning rx thread");
        return RET_ERROR_NO_MEM;
    }

    LOG_INF("CAN bus init ok: TX addr: 0x%x, RX addr: 0x%x", tx_addr.std_id, rx_addr.std_id);

    return RET_SUCCESS;
}
