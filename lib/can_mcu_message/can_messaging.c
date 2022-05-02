#include "can_messaging.h"
#include "canbus_rx.h"
#include "canbus_tx.h"
#include <drivers/can.h>
#include <kernel.h>
#include <logging/log.h>
#include <sys/__assert.h>
LOG_MODULE_REGISTER(can_messaging);

#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY   10

struct k_thread poll_state_thread_data;
K_THREAD_STACK_DEFINE(poll_state_stack, STATE_POLL_THREAD_STACK_SIZE);

void
poll_state_thread(void *unused1, void *unused2, void *unused3)
{
    struct can_bus_err_cnt err_cnt = {0, 0};
    struct can_bus_err_cnt err_cnt_prev = {0, 0};
    enum can_state state_prev = CAN_ERROR_ACTIVE;
    enum can_state state;
    int err;

    static const struct device *can_dev =
        DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    if (!can_dev) {
        LOG_ERR("CAN: Device driver not found.");
        return;
    }

    while (1) {
        err = can_get_state(can_dev, &state, &err_cnt);
        if (err != 0) {
            LOG_ERR("Failed to get CAN controller state: %d", err);
            k_sleep(K_MSEC(100));
            continue;
        }

        if (err_cnt.tx_err_cnt != err_cnt_prev.tx_err_cnt ||
            err_cnt.rx_err_cnt != err_cnt_prev.rx_err_cnt ||
            state_prev != state) {

            err_cnt_prev.tx_err_cnt = err_cnt.tx_err_cnt;
            err_cnt_prev.rx_err_cnt = err_cnt.rx_err_cnt;
            state_prev = state;
            LOG_DBG("state: %u, rx error count: %d, tx error count: %d", state,
                    err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
        } else {
            k_sleep(K_MSEC(100));
        }
    }
}

ret_code_t
can_messaging_init(void (*in_handler)(McuMessage *msg))
{
    ret_code_t err_code;

    // init underlying layers: CAN bus
    err_code = canbus_rx_init(in_handler);
    APP_ASSERT(err_code);

    err_code = canbus_tx_init();
    APP_ASSERT(err_code);

    k_tid_t tid = k_thread_create(&poll_state_thread_data, poll_state_stack,
                                  K_THREAD_STACK_SIZEOF(poll_state_stack),
                                  poll_state_thread, NULL, NULL, NULL,
                                  STATE_POLL_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!tid) {
        LOG_ERR("ERROR spawning poll_state_thread");
    }

    LOG_INF("CAN bus init ok");

    return RET_SUCCESS;
}
