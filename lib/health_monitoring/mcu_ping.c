//
// Created by Cyril Fougeray on 17/10/2025.
//

#include "include/mcu_ping.h"
#include <errors.h>
#include <orb_logs.h>
#include <stdlib.h>

static uint32_t ping_last_sent_counter = 0;
static bool waiting_for_pong = false;
const uint8_t ping_test_bytes[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; //  "Hello"
static int (*send_cb)(orb_mcu_Ping *ping);

LOG_MODULE_REGISTER(ping);

int
ping_received(orb_mcu_Ping *ping)
{
    if (waiting_for_pong && ping->counter == ping_last_sent_counter) {
        // response to our last ping
        LOG_INF("Received pong response from mcu");
        // check content
        if (ping->test.size != sizeof(ping_test_bytes) ||
            memcmp(ping->test.bytes, ping_test_bytes,
                   sizeof(ping_test_bytes)) != 0) {
            LOG_ERR("Invalid ping response from mcu");
            return RET_ERROR_INVALID_PARAM;
        }

        waiting_for_pong = false;
        return RET_SUCCESS;
    }

    // Handle ping from remote mcu
    return ping_pong_send_mcu(ping);
}

bool
pong_received(void)
{
    return (waiting_for_pong == false);
}

void
ping_pong_reset()
{
    waiting_for_pong = false;
}

int
ping_pong_send_mcu(orb_mcu_Ping *ping)
{
    if (ping == NULL && waiting_for_pong) {
        return RET_ERROR_BUSY;
    }
    if (send_cb == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    // Create a ping or pong message to send to remote MCU
    orb_mcu_Ping ping_pong = {0};

    if (ping) {
        // respond to a ping from remote MCU
        ping_pong.counter = ping->counter;
        ping_pong.test.size = ping->test.size;
        memcpy(&ping_pong.test.bytes, &ping->test.bytes,
               MIN(sizeof(ping_pong.test.bytes), ping_pong.test.size));
        LOG_INF("Responding pong to mcu (counter: %u)", ping_pong.counter);
    } else {
        // send a new ping to remote mcu
        ping_pong.counter = rand() % UINT32_MAX;
        ping_pong.test.size = sizeof(ping_test_bytes);
        memcpy(&ping_pong.test.bytes, ping_test_bytes, sizeof(ping_test_bytes));

        waiting_for_pong = true;
        ping_last_sent_counter = ping_pong.counter;
        LOG_INF("Sending ping to mcu (counter: %u)", ping_pong.counter);
    }

    int ret = send_cb(&ping_pong);
    return ret;
}

void
ping_init(int (*send_fn)(orb_mcu_Ping *ping))
{
    send_cb = send_fn;
}
