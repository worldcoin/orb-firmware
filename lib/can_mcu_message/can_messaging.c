#include "can_messaging.h"
#include "canbus_rx.h"
#include "canbus_tx.h"
#include <logging/log.h>
#include <sys/__assert.h>
LOG_MODULE_REGISTER(can_messaging);

ret_code_t
can_messaging_init(void (*in_handler)(McuMessage *msg))
{
    ret_code_t err_code;

    // init underlying layers: CAN bus
    err_code = canbus_rx_init(in_handler);
    APP_ASSERT(err_code);

    err_code = canbus_tx_init();
    APP_ASSERT(err_code);

    LOG_INF("CAN bus init ok");

    return RET_SUCCESS;
}
