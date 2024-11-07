#include "can_messaging.h"
#include "canbus_rx.h"
#include "canbus_tx.h"
#include "orb_logs.h"
#include <app_assert.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/can/can_mcan.h>
#include <zephyr/kernel.h>
LOG_MODULE_REGISTER(can_messaging, CONFIG_CAN_MESSAGING_LOG_LEVEL);

static const struct device *can_dev =
    DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_canbus));

#if !DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_canbus), okay)
#warning CAN device not enabled, you can deselect CONFIG_ORB_LIB_CAN_MESSAGING
#endif

static K_THREAD_STACK_DEFINE(can_monitor_stack_area,
                             CONFIG_ORB_LIB_THREAD_STACK_SIZE_CANBUS_MONITOR);
static struct k_thread can_monitor_thread_data;
static k_tid_t can_monitor_tid = NULL;
#define CAN_MONITOR_INITIAL_INTERVAL_MS 10000
#define CAN_MONITOR_ERROR_STATE_INTERVAL_MS                                    \
    2000 // poll & recover more often on error

static struct can_bus_err_cnt current_err_cnt;
static struct k_work can_reset_work = {0};

/// Queue state_change_work_handler
static void
state_change_callback(const struct device *dev, enum can_state state,
                      struct can_bus_err_cnt err_cnt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(state);
    ARG_UNUSED(err_cnt);
    ARG_UNUSED(user_data);

    LOG_INF("CAN bus state changed, state: %d->%d, "
            "rx error count: %u, "
            "tx error count: %u",
            state, state, err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);

    k_wakeup(can_monitor_tid);
}

/// Thread needed to ensure CAN doesn't stay in BUS_OFF state
_Noreturn static void
can_monitor_thread()
{
    uint32_t off_recover_count = 0;
    uint32_t delay = CAN_MONITOR_INITIAL_INTERVAL_MS;
    enum can_state current_state;
    int ret;

    while (true) {
        k_sleep(K_MSEC(delay));

        if (can_dev == NULL) {
            continue;
        }

        can_mcan_get_state(can_dev, &current_state, &current_err_cnt);
        if (current_state == CAN_STATE_BUS_OFF) {
            LOG_WRN("CAN recovery from bus-off");

            (void)can_stop(can_dev);
            k_msleep(500);

            ret = can_start(can_dev);
            if (ret != -EALREADY) {
                ASSERT_HARD(ret);
            }
            k_msleep(500);

            off_recover_count++;
            if (off_recover_count > 10) {
                ASSERT_HARD_BOOL(false);
            }

            ret = can_recover(can_dev, K_MSEC(2000));
            if (ret != 0) {
                LOG_ERR("CAN recovery failed: %d", ret);
            }

            // check again soon if off state persists
            delay = CAN_MONITOR_ERROR_STATE_INTERVAL_MS;

            // reset TX queues and buffers
            canbus_tx_init();
            canbus_isotp_tx_init();
        } else if (current_state <= CAN_STATE_ERROR_WARNING ||
                   current_state == CAN_STATE_STOPPED) {
            off_recover_count = 0;
            delay = CAN_MONITOR_INITIAL_INTERVAL_MS;
        } else {
            // CAN_STATE_ERROR_PASSIVE
            delay = CAN_MONITOR_ERROR_STATE_INTERVAL_MS;
        }
    }
}

static void
can_reset_work_handler(struct k_work *work)
{
    UNUSED_PARAMETER(work);

    LOG_INF("CAN bus reset");

    // reinit TX queues and thread state
    int err_code = canbus_tx_init();
    ASSERT_HARD(err_code);
    err_code = canbus_isotp_tx_init();
    ASSERT_HARD(err_code);
}

/**
 * Reset CAN TX queues, keep RX threads running
 * Can be used in ISR context
 * @return RET_SUCCESS on success, error code otherwise
 */
static ret_code_t
can_messaging_reset_async(void)
{
    // check that the job is initialized by reading the handler value
    if (can_reset_work.handler != NULL && k_work_submit(&can_reset_work) < 0) {
        return RET_ERROR_INTERNAL;
    } else if (can_reset_work.handler == NULL) {
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}

ret_code_t
can_messaging_suspend(void)
{
    int err_code = RET_SUCCESS;

    if (can_dev) {
        err_code = can_stop(can_dev);
        if (err_code != -EALREADY) {
            ASSERT_HARD(err_code);
        }
    }

    return err_code;
}

ret_code_t
can_messaging_resume(void)
{
    int err_code;

    // reset CAN queues
    err_code = can_messaging_reset_async();
    ASSERT_HARD(err_code);

    if (can_dev) {
        err_code = can_start(can_dev);
        if (err_code != -EALREADY) {
            ASSERT_HARD(err_code);
        }

        // ensure correct CAN state
        k_wakeup(can_monitor_tid);
    }

    return RET_SUCCESS;
}

ret_code_t
can_messaging_init(ret_code_t (*in_handler)(can_message_t *message))
{
    uint32_t err_code;

    if (can_dev == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    // enable CAN-FD
    int ret = can_set_mode(can_dev, CAN_MODE_FD);
    if (ret) {
        ASSERT_SOFT(ret);
        return ret;
    }

    // init underlying layers: CAN bus
    err_code = canbus_rx_init(in_handler);
    ASSERT_SOFT(err_code);

    err_code |= canbus_isotp_rx_init(in_handler);
    ASSERT_SOFT(err_code);

    err_code |= canbus_tx_init();
    ASSERT_SOFT(err_code);

    err_code |= canbus_isotp_tx_init();
    ASSERT_SOFT(err_code);

    // set up CAN-monitoring thread
    if (can_monitor_tid == NULL) {
        can_monitor_tid = k_thread_create(
            &can_monitor_thread_data, can_monitor_stack_area,
            K_THREAD_STACK_SIZEOF(can_monitor_stack_area), can_monitor_thread,
            NULL, NULL, NULL, CONFIG_ORB_LIB_THREAD_PRIORITY_CANBUS_MONITOR, 0,
            K_NO_WAIT);
        k_thread_name_set(&can_monitor_thread_data, "can_mon");
    }

    // set up CAN-state change callback
    // /!\ doesn't seem to be 100% reliable, thus the need for the
    // CAN-monitoring thread
    can_set_state_change_callback(can_dev, state_change_callback, NULL);
    // set up work handler for CAN reset
    k_work_init(&can_reset_work, can_reset_work_handler);

    if (err_code == 0) {
        err_code = can_start(can_dev);
        ASSERT_SOFT(err_code);
    } else {
        err_code = RET_ERROR_INTERNAL;
    }

    return err_code;
}
