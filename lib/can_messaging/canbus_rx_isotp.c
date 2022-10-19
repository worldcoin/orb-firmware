#include "can_messaging.h"
#include <app_assert.h>
#include <assert.h>
#include <canbus/isotp.h>
#include <device.h>
#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(isotp_rx, CONFIG_ISOTP_RX_LOG_LEVEL);

#define ISOTP_FLOWCTRL_BS 8

const struct device *can_dev;
const struct isotp_fc_opts flow_control_opts = {.bs = ISOTP_FLOWCTRL_BS,
                                                .stmin = 0};

K_THREAD_STACK_DEFINE(isotp_rx_thread_stack,
                      CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_ISOTP_RX);
static struct k_thread rx_thread_data = {0};

static struct isotp_recv_ctx rx_ctx[1 + CONFIG_CAN_ISOTP_REMOTE_APP_COUNT] = {
    0};
static struct k_poll_event poll_evt[1 + CONFIG_CAN_ISOTP_REMOTE_APP_COUNT] = {
    0};

static void (*incoming_message_handler)(void *msg);

static_assert(CONFIG_CAN_ISOTP_REMOTE_APP_COUNT <= 15,
              "ISO-TP binding allowed to a maximum of 15 apps");
static_assert(CONFIG_ISOTP_RX_SF_FF_BUF_COUNT >=
                  (CONFIG_CAN_ISOTP_REMOTE_APP_COUNT + 1),
              "Not enough receiving buffers configured for the ISO-TP module");

// one buffer takes ISOTP_FLOWCTRL_BS*7
static_assert(CONFIG_ISOTP_RX_BUF_COUNT * ISOTP_FLOWCTRL_BS * 7 >= 541,
              "We need enough buffers to receive 512-byte long messages");

static void
bind_to_remotes(void)
{
    int ret;

    // bind to Jetson->MCU messages
    // remote 0x8 is Jetson, IDs above are Jetson apps
    for (uint32_t app_id = 0; app_id <= CONFIG_CAN_ISOTP_REMOTE_APP_COUNT;
         ++app_id) {
        struct isotp_msg_id app_to_mcu_dst_addr = {
            .std_id = CAN_ISOTP_STDID_DESTINATION(
                (CONFIG_CAN_ISOTP_REMOTE_ID + app_id),
                CONFIG_CAN_ISOTP_LOCAL_ID),
            .id_type = CAN_STANDARD_IDENTIFIER,
            .use_ext_addr = 0};
        struct isotp_msg_id app_to_mcu_src_addr = {
            .std_id =
                CAN_ISOTP_STDID_SOURCE((CONFIG_CAN_ISOTP_REMOTE_ID + app_id),
                                       CONFIG_CAN_ISOTP_LOCAL_ID),
            .id_type = CAN_STANDARD_IDENTIFIER,
            .use_ext_addr = 0};

        ret = isotp_bind(&rx_ctx[app_id], can_dev, &app_to_mcu_dst_addr,
                         &app_to_mcu_src_addr, &flow_control_opts, K_FOREVER);
        ASSERT_SOFT_BOOL(ISOTP_N_OK == ret);

        // initialize poll for the rx_ctx
        k_poll_event_init(&poll_evt[app_id], K_POLL_TYPE_FIFO_DATA_AVAILABLE,
                          K_POLL_MODE_NOTIFY_ONLY, &rx_ctx[app_id].fifo);
    }
}

_Noreturn static void
jetson_to_mcu_rx_thread()
{
    struct net_buf *buf = NULL;
    int ret, rem_len;
    size_t wr_idx = 0;
    static can_message_t rx_message = {0};
    static uint8_t buffer[CONFIG_CAN_ISOTP_MAX_SIZE_BYTES];

    // listen remotes
    bind_to_remotes();
    while (1) {
        // wait for any event on all the RX contexts
        ret = k_poll(poll_evt, ARRAY_SIZE(poll_evt), K_FOREVER);

        if (ret != 0) {
            LOG_ERR("ISO-TP rx error, k_poll ret %i", ret);

            if (ret == -EINTR) {
                // one of the k_poll event is K_POLL_STATE_CANCELLED
                // reset states and wait for new k_poll event
                // one message will be lost
                for (uint32_t app_id = 0;
                     app_id <= CONFIG_CAN_ISOTP_REMOTE_APP_COUNT; ++app_id) {
                    poll_evt[app_id].state = K_POLL_STATE_NOT_READY;
                }
            }

            continue;
        }

        // check all poll states to handle incoming data
        for (uint32_t app_id = 0; app_id <= CONFIG_CAN_ISOTP_REMOTE_APP_COUNT;
             ++app_id) {
            if (poll_evt[app_id].state == K_POLL_STATE_DATA_AVAILABLE) {
                LOG_DBG("Handling rx_ctx #%u, dest 0x%x", app_id,
                        rx_ctx[app_id].rx_addr.std_id);

                // reset wr_idx to copy into fresh receiving buffer
                wr_idx = 0;

                // enter receiving loop
                // we will not exit until all the bytes are received or timeout
                do {
                    // get new block (BS)
                    buf = NULL;
                    rem_len = isotp_recv_net(&rx_ctx[app_id], &buf, K_FOREVER);
                    if (rem_len < ISOTP_N_OK) {
                        LOG_ERR("ISO-TP rx error: %d", rem_len);
                        // destroy allocated buffer if it exists
                        if (buf != NULL) {
                            net_buf_unref(buf);
                        }
                        break;
                    }

                    if (wr_idx + buf->len <= sizeof(buffer)) {
                        memcpy(&buffer[wr_idx], buf->data, buf->len);
                        wr_idx += buf->len;
                    } else {
                        ASSERT_SOFT(RET_ERROR_NO_MEM);
                        LOG_ERR("CAN message too long: %u", wr_idx + buf->len);
                    }

                    net_buf_unref(buf);
                } while (rem_len > 0);

                LOG_DBG("Received %u bytes", wr_idx);
                if (rem_len == ISOTP_N_OK) {
                    // push for processing and keep destination ID to send
                    // any response to the sender
                    rx_message.size = wr_idx;
                    rx_message.bytes = buffer;
                    rx_message.destination = rx_ctx[app_id].rx_addr.std_id;
                    if (incoming_message_handler != NULL) {
                        incoming_message_handler((void *)&rx_message);
                    } else {
                        LOG_ERR("Cannot handle message");
                    }
                } else {
                    LOG_DBG("Data not received: %d", rem_len);
                }
            }

            // in any case, reset state of current poll event
            poll_evt[app_id].state = K_POLL_STATE_NOT_READY;
        }
    }
}

ret_code_t
canbus_isotp_rx_init(void (*in_handler)(void *msg))
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

    k_tid_t tid =
        k_thread_create(&rx_thread_data, isotp_rx_thread_stack,
                        K_THREAD_STACK_SIZEOF(isotp_rx_thread_stack),
                        jetson_to_mcu_rx_thread, NULL, NULL, NULL,
                        CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_RX, 0, K_NO_WAIT);
    k_thread_name_set(tid, "can_isotp_rx");

    return RET_SUCCESS;
}
