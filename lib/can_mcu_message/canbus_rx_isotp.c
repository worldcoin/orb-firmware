#include "can_messaging.h"
#include "mcu_messaging.pb.h"
#include <app_assert.h>
#include <canbus/isotp.h>
#include <device.h>
#include <pb.h>
#include <pb_decode.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(isotp_rx);

const struct device *can_dev;
const struct isotp_fc_opts flow_control_opts = {.bs = 8, .stmin = 0};

K_THREAD_STACK_DEFINE(isotp_rx_thread_stack,
                      CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_ISOTP_RX);
static struct k_thread rx_thread_data = {0};

const struct isotp_msg_id jetson_to_mcu_dst_addr = {
    .std_id = CAN_ISOTP_STDID_DESTINATION(CONFIG_CAN_ISOTP_REMOTE_ID,
                                          CONFIG_CAN_ISOTP_LOCAL_ID),
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0};
const struct isotp_msg_id jetson_to_mcu_src_addr = {
    .std_id = CAN_ISOTP_STDID_SOURCE(CONFIG_CAN_ISOTP_REMOTE_ID,
                                     CONFIG_CAN_ISOTP_LOCAL_ID),
    .id_type = CAN_STANDARD_IDENTIFIER,
    .use_ext_addr = 0};

static void (*incoming_message_handler)(McuMessage *msg);
#define RX_BUF_SIZE (McuMessage_size + 1)
static uint8_t rx_buffer[RX_BUF_SIZE] = {0};

_Noreturn static void
jetson_to_mcu_rx_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct net_buf *buf = NULL;
    int ret, rem_len;
    struct isotp_recv_ctx recv_ctx = {0};
    size_t wr_idx = 0;

    // set CAN type for incoming message handler to respond
    // using the same transport
    k_thread_custom_data_set((void *)CAN_ISOTP);

    ret = isotp_bind(&recv_ctx, can_dev, &jetson_to_mcu_dst_addr,
                     &jetson_to_mcu_src_addr, &flow_control_opts, K_FOREVER);
    ASSERT_SOFT_BOOL(ISOTP_N_OK == ret);

    while (1) {
        wr_idx = 0;

        // enter receiving loop
        // we will not exit until all the bytes are received or timeout
        do {
            // get new block (BS)
            rem_len = isotp_recv_net(&recv_ctx, &buf, K_FOREVER);
            if (rem_len < ISOTP_N_OK) {
                LOG_DBG("Receiving error [%d]", rem_len);
                break;
            }

            if (wr_idx + buf->len <= sizeof(rx_buffer)) {
                memcpy(&rx_buffer[wr_idx], buf->data, buf->len);
                wr_idx += buf->len;
            } else {
                ASSERT_SOFT(RET_ERROR_NO_MEM);
                LOG_ERR("CAN message too long: %u", wr_idx + buf->len);
            }

            memset(buf->data, 0, buf->len);
            net_buf_unref(buf);
        } while (rem_len > 0);

        LOG_DBG("Received %u bytes", wr_idx);
        if (rem_len == ISOTP_N_OK) {
            pb_istream_t stream = pb_istream_from_buffer(rx_buffer, wr_idx);
            McuMessage data = {0};

            bool decoded = pb_decode_ex(&stream, McuMessage_fields, &data,
                                        PB_DECODE_DELIMITED);
            if (decoded) {
                if (incoming_message_handler != NULL) {
                    incoming_message_handler(&data);
                } else {
                    LOG_ERR("Cannot handle message");
                }
            } else {
                LOG_ERR("Error parsing data, discarding");
            }
        } else {
            LOG_DBG("Data not received: %d", rem_len);
        }
    }
}

ret_code_t
canbus_isotp_rx_init(void (*in_handler)(McuMessage *msg))
{
    if (in_handler == NULL) {
        return RET_ERROR_INVALID_PARAM;
    } else {
        incoming_message_handler = in_handler;
    }

    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return RET_ERROR_NOT_FOUND;
    }

    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN not ready");
        return RET_ERROR_BUSY;
    } else {
        LOG_INF("CAN ready");
    }

    k_thread_create(&rx_thread_data, isotp_rx_thread_stack,
                    K_THREAD_STACK_SIZEOF(isotp_rx_thread_stack),
                    jetson_to_mcu_rx_thread, NULL, NULL, NULL,
                    CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_RX, 0, K_NO_WAIT);

    return RET_SUCCESS;
}
