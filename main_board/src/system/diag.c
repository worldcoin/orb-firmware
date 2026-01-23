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

static orb_mcu_HardwareState_Status
to_pb_status(const ret_code_t ret)
{
    switch (ret) {
    case RET_SUCCESS:
        return orb_mcu_HardwareState_Status_STATUS_SUCCESS;
    case RET_ERROR_INTERNAL:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_INTERNAL;
    case RET_ERROR_NO_MEM:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_NO_MEM;
    case RET_ERROR_NOT_FOUND:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_NOT_FOUND;
    case RET_ERROR_INVALID_PARAM:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_INVALID_PARAM;
    case RET_ERROR_INVALID_STATE:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_INVALID_STATE;
    case RET_ERROR_INVALID_ADDR:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_INVALID_ADDR;
    case RET_ERROR_BUSY:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_BUSY;
    case RET_ERROR_OFFLINE:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_OFFLINE;
    case RET_ERROR_FORBIDDEN:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_FORBIDDEN;
    case RET_ERROR_TIMEOUT:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_TIMEOUT;
    case RET_ERROR_NOT_INITIALIZED:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_NOT_INITIALIZED;
    case RET_ERROR_ASSERT_FAILS:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_ASSERT_FAILS;
    case RET_ERROR_ALREADY_INITIALIZED:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_ALREADY_INITIALIZED;
    case RET_ERROR_NOT_SUPPORTED:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_NOT_SUPPORTED;
    case RET_ERROR_UNSAFE:
        return orb_mcu_HardwareState_Status_STATUS_ERROR_UNSAFE;
    }

    return orb_mcu_HardwareState_Status_STATUS_ERROR_INTERNAL;
}

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
            if (more_data) {
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
        memset(&hw_state, 0, sizeof(hw_state));
        memccpy(hw_state.source_name, data->name, '\0',
                sizeof(hw_state.source_name));
        hw_state.status = to_pb_status(data->dynamic_data->status);
        memccpy(hw_state.message, data->dynamic_data->message, '\0',
                sizeof(hw_state.message));
        ret = publish_new(&hw_state, sizeof(hw_state),
                          orb_mcu_main_McuToJetson_hw_state_tag, remote);
        if (ret) {
            error_counter++;
            continue;
        }
        counter++;

        // throttle the sending of statuses to avoid flooding the CAN bus
        // and CAN controller
        k_msleep(10);
    }
    LOG_INF("Sent: %u, errors: %u", counter, error_counter);

    return ret;
}

#if defined(CONFIG_ZTEST)
#include <zephyr/ztest.h>

ZTEST(hil, test_diag_sync) { diag_sync(CONFIG_CAN_ADDRESS_MCU_TO_JETSON_TX); }
#endif
