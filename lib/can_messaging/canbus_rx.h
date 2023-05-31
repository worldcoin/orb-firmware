#pragma once

#include "can_messaging.h"

ret_code_t canbus_rx_init(ret_code_t (*in_handler)(void *message));

ret_code_t canbus_isotp_rx_init(ret_code_t (*in_handler)(void *message));
