#include "can_messaging.h"
#include "canbus_rx.h"
#include "canbus_tx.h"
#include <app_assert.h>
#include <drivers/can.h>
#include <kernel.h>
#include <logging/log.h>
#include <sys/__assert.h>
LOG_MODULE_REGISTER(can_messaging);

static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static enum can_state current_state;
static struct can_bus_err_cnt current_err_cnt;
static struct k_work state_change_work;

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
    LOG_INF("CAN bus state changed, state: %d, "
            "rx error count: %u, "
            "tx error count: %u",
            current_state, current_err_cnt.rx_err_cnt,
            current_err_cnt.tx_err_cnt);

    if (current_state == CAN_BUS_OFF) {
        LOG_WRN("CAN recovery from bus-off");

        if (can_recover(can_dev, K_MSEC(2000)) != 0) {
            ASSERT_HARD(RET_ERROR_OFFLINE);
        }
    }
}

ret_code_t
can_messaging_init(void (*in_handler)(McuMessage *msg))
{
    int err_code;

    // init underlying layers: CAN bus
    err_code = canbus_rx_init(in_handler);
    ASSERT_SOFT(err_code);

    err_code = canbus_isotp_rx_init(in_handler);
    ASSERT_SOFT(err_code);

    err_code = canbus_tx_init();
    ASSERT_SOFT(err_code);

    err_code = canbus_isotp_tx_init();
    ASSERT_SOFT(err_code);

    // set up handler for CAN bus state change
    k_work_init(&state_change_work, state_change_work_handler);
    can_set_state_change_callback(can_dev, state_change_callback,
                                  &state_change_work);

    return RET_SUCCESS;
}
