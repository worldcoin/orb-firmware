#include "include/mcu_ping.h"
#include <errors.h>
#include <orb_logs.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/sys/util.h>

typedef enum {
    PING_STATE_IDLE,
    PING_STATE_WAITING_FOR_PONG,
    PING_STATE_PONG_RECEIVED,
} ping_state_t;

static uint32_t ping_last_sent_counter = 0;
static ping_state_t ping_state = PING_STATE_IDLE;
const uint8_t ping_test_bytes[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; //  "Hello"
static int (*send_cb)(orb_mcu_Ping *ping);

LOG_MODULE_REGISTER(ping);

int
ping_received(orb_mcu_Ping *ping)
{
    if (ping == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (ping_state == PING_STATE_WAITING_FOR_PONG &&
        ping->counter == ping_last_sent_counter) {
        // response to our last ping
        LOG_INF("Received pong response from mcu");
        // check content
        if (ping->test.size != sizeof(ping_test_bytes) ||
            memcmp(ping->test.bytes, ping_test_bytes,
                   sizeof(ping_test_bytes)) != 0) {
            LOG_WRN("Invalid ping response from mcu");
            return RET_ERROR_INVALID_PARAM;
        }

        ping_state = PING_STATE_PONG_RECEIVED;
        return RET_SUCCESS;
    }

    // Handle ping from remote mcu
    return ping_pong_send_mcu(ping);
}

bool
pong_received(void)
{
    return (ping_state == PING_STATE_PONG_RECEIVED);
}

void
ping_pong_reset()
{
    ping_state = PING_STATE_IDLE;
}

int
ping_pong_send_mcu(orb_mcu_Ping *ping)
{
    if (ping == NULL && ping_state == PING_STATE_WAITING_FOR_PONG) {
        return RET_ERROR_BUSY;
    }
    if (send_cb == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }
    if (ping && ping->test.size > sizeof(ping->test.bytes)) {
        return RET_ERROR_INVALID_PARAM;
    }

    // Create a ping or pong message to send to remote MCU
    orb_mcu_Ping ping_pong = {0};
    ping_state_t previous_state = ping_state;

    if (ping) {
        // respond to a ping from remote MCU
        ping_pong.counter = ping->counter;
        ping_pong.test.size = ping->test.size;
        memcpy(&ping_pong.test.bytes, &ping->test.bytes,
               MIN(sizeof(ping_pong.test.bytes), ping_pong.test.size));
        LOG_INF("Responding pong to mcu (counter: %u)", ping_pong.counter);
    } else {
        // send a new ping to remote mcu
        // Reset state if we were in PONG_RECEIVED state
        if (ping_state == PING_STATE_PONG_RECEIVED) {
            ping_state = PING_STATE_IDLE;
        }

        ping_pong.counter = rand() % UINT32_MAX;
        ping_pong.test.size = sizeof(ping_test_bytes);
        memcpy(&ping_pong.test.bytes, ping_test_bytes, sizeof(ping_test_bytes));

        previous_state = ping_state;
        ping_state = PING_STATE_WAITING_FOR_PONG;
        ping_last_sent_counter = ping_pong.counter;
        LOG_INF("Sending ping to mcu (counter: %u)", ping_pong.counter);
    }

    int ret = send_cb(&ping_pong);
    if (ret != RET_SUCCESS) {
        // Rollback state if send failed and we were sending a new ping
        if (ping == NULL) {
            ping_state = previous_state;
            LOG_WRN("Failed to send ping to mcu: %d", ret);
        } else {
            LOG_WRN("Failed to send pong response to mcu: %d", ret);
        }
    }
    return ret;
}

void
ping_init(int (*send_fn)(orb_mcu_Ping *ping))
{
    send_cb = send_fn;
}
