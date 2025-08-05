#include "diag.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <errors.h>
#include <orb_state.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_MEMFAULT)
#include <memfault/components.h>

static orb_mcu_MemfaultEvent mflt_evt = orb_mcu_MemfaultEvent_init_zero;
static bool mflt_evt_synced = true;

/// check that the buffer is big enough to hold the minimum chunk size required
/// by the Memfault packetizer
BUILD_ASSERT(ARRAY_SIZE(mflt_evt.chunk.bytes) >
             MEMFAULT_PACKETIZER_MIN_BUF_LEN);
#endif

#if defined(CONFIG_ZTEST)
#include <zephyr/logging/log.h>
#else
#include "orb_logs.h"
#endif
LOG_MODULE_REGISTER(diag, CONFIG_DIAG_LOG_LEVEL);

BUILD_ASSERT(ORB_STATE_MESSAGE_MAX_LENGTH ==
                 sizeof(((orb_mcu_HardwareState *)0)->message),
             "orb_state message length must match orb_mcu_HardwareState "
             "message length");
BUILD_ASSERT(ORB_STATE_NAME_MAX_LENGTH ==
                 sizeof(((orb_mcu_HardwareState *)0)->source_name),
             "orb_state name length must match orb_mcu_HardwareState "
             "source_name length");

ret_code_t
diag_sync(uint32_t remote)
{
    int ret = RET_SUCCESS;

    // memfault first, then general statuses, see below

#if defined(CONFIG_MEMFAULT)
    static uint32_t memfault_counter = 0;

    if (memfault_packetizer_data_available()) {
        LOG_INF("Sending Memfault events");

        bool more_data = false;
        do {
            if (mflt_evt_synced) {
                mflt_evt_synced = false;
                size_t buf_size = ARRAY_SIZE(mflt_evt.chunk.bytes);
                more_data = memfault_packetizer_get_chunk(mflt_evt.chunk.bytes,
                                                          &buf_size);
                mflt_evt.chunk.size = buf_size;
                mflt_evt.counter = memfault_counter;
            }
#ifndef DEBUG
            ret = publish_new(&mflt_evt, sizeof(mflt_evt),
                              orb_mcu_main_McuToJetson_memfault_event_tag,
                              remote);
            if (ret) {
                // come back later to send the same chunk
                return ret;
            }
#endif
            memfault_counter++;
            mflt_evt_synced = true;

            // throttle the sending of statuses to avoid flooding the CAN bus
            // and CAN controller
            if (!IS_ENABLED(CONFIG_ZTEST) && more_data) {
                k_msleep(10);
            }
        } while (more_data);
    }
#endif

    size_t counter = 0;
    size_t error_counter = 0;
    orb_mcu_HardwareState hw_state = orb_mcu_HardwareState_init_zero;

    LOG_INF("Sending statuses");

    struct orb_state_const_data *data = NULL;
    while (orb_state_iter(&data)) {
        memccpy(hw_state.source_name, data->name, '\0',
                sizeof(hw_state.source_name));
        hw_state.status = data->dynamic_data->status;
        memccpy(hw_state.message, data->dynamic_data->message, '\0',
                sizeof(hw_state.message));
        ret = publish_new(&hw_state, sizeof(hw_state),
                          orb_mcu_main_McuToJetson_hw_state_tag, remote);
        if (ret) {
            error_counter++;
            continue;
        }
        counter++;

#ifndef CONFIG_ZTEST
        // throttle the sending of statuses to avoid flooding the CAN bus
        // and CAN controller
        k_msleep(10);
#endif
    }
    LOG_INF("Sent: %u, errors: %u", counter, error_counter);

    return ret;
}

#if defined(CONFIG_ZTEST)
#include <zephyr/ztest.h>

ZTEST(hil, test_diag_sync) { diag_sync(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE); }
#endif
