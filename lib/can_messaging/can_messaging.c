#include "can_messaging.h"
#include "canbus_rx.h"
#include "canbus_tx.h"
#include <app_assert.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(can_messaging, CONFIG_CAN_MESSAGING_LOG_LEVEL);

static const struct device *can_dev =
    DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_canbus));

#if !DT_NODE_HAS_STATUS(DT_CHOSEN(zephyr_canbus), okay)
#warning CAN device not enabled, you can deselect CONFIG_ORB_LIB_CAN_MESSAGING
#endif

static enum can_state current_state;
static struct can_bus_err_cnt current_err_cnt;
static struct k_work state_change_work = {0};
static struct k_work can_reset_work = {0};

/// Queue state_change_work_handler
static void
state_change_callback(const struct device *dev, enum can_state state,
                      struct can_bus_err_cnt err_cnt, void *user_data)
{
    struct k_work *work = (struct k_work *)user_data;

    ARG_UNUSED(dev);

    current_state = state;
    current_err_cnt = err_cnt;
    k_work_submit(work);
}

/// Print out CAN bus state change
/// Recover manually in case CONFIG_CAN_AUTO_BUS_OFF_RECOVERY not defined
/// \param work
static void
state_change_work_handler(struct k_work *work)
{
    UNUSED_PARAMETER(work);

    LOG_INF("CAN bus state changed, state: %d, "
            "rx error count: %u, "
            "tx error count: %u",
            current_state, current_err_cnt.rx_err_cnt,
            current_err_cnt.tx_err_cnt);

    if (current_state == CAN_STATE_BUS_OFF) {
        LOG_WRN("CAN recovery from bus-off");

        if (can_dev != NULL && can_recover(can_dev, K_MSEC(2000)) != 0) {
            ASSERT_HARD(RET_ERROR_OFFLINE);
        } else {
            // reset TX queues and buffers
            canbus_tx_init();
            canbus_isotp_tx_init();
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

/// Reset CAN TX queues, keep RX threads running
/// Can be used in ISR context
/// \return RET_SUCCESS on success, error code otherwise
static ret_code_t
can_messaging_reset_async(void)
{
    // check that the job is initialized by reading the handler value
    if (can_reset_work.handler != NULL && k_work_submit(&can_reset_work) < 0) {
        return RET_ERROR_INTERNAL;
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
            ASSERT_SOFT(err_code);
            err_code = RET_ERROR_INTERNAL;
        }
    }

    return err_code;
}

ret_code_t
can_messaging_resume(void)
{
    int err_code = RET_SUCCESS;

    // reset CAN queues
    err_code = can_messaging_reset_async();
    ASSERT_SOFT(err_code);

    if (can_dev) {
        err_code = can_start(can_dev);
        if (err_code != -EALREADY) {
            ASSERT_SOFT(err_code);
            err_code = RET_ERROR_INTERNAL;
        }
    }

    return err_code;
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

    // set up handler for CAN bus state change
    k_work_init(&state_change_work, state_change_work_handler);
    can_set_state_change_callback(can_dev, state_change_callback,
                                  &state_change_work);

    // set up work handler for CAN reset
    k_work_init(&can_reset_work, can_reset_work_handler);

    if (err_code == 0) {
        err_code = can_start(can_dev);
        ASSERT_SOFT(err_code);
    }

    return RET_SUCCESS;
}
