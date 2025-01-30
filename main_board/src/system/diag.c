#include "diag.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <errors.h>
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

static orb_mcu_HardwareDiagnostic_Status
    hw_statuses[orb_mcu_HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL];
static bool has_changed;

bool
diag_has_data(void)
{
    bool sync_memfault = false;

#if defined(CONFIG_MEMFAULT)
    sync_memfault =
        (memfault_packetizer_data_available() > 0) || !mflt_evt_synced;
#endif

    return has_changed || sync_memfault;
}

ret_code_t
diag_sync(uint32_t remote)
{
    int ret = RET_SUCCESS;
    size_t counter = 0;
    size_t error_counter = 0;
    orb_mcu_HardwareDiagnostic hw_diag = orb_mcu_HardwareDiagnostic_init_zero;

    if (has_changed) {
        LOG_INF("Sending statuses");

        for (size_t i = 0; i < ARRAY_SIZE(hw_statuses); i++) {
            if (hw_statuses[i] ==
                orb_mcu_HardwareDiagnostic_Status_STATUS_UNKNOWN) {
                continue;
            }
            hw_diag.source = i;
            hw_diag.status = hw_statuses[i];

            ret =
                publish_new(&hw_diag, sizeof(hw_diag),
                            orb_mcu_main_McuToJetson_hardware_diag_tag, remote);
            if (ret) {
#ifndef CONFIG_ZTEST
                error_counter++;
#endif
                continue;
            }
            counter++;

            // throttle the sending of statuses to avoid flooding the CAN bus
            // and CAN controller
            k_msleep(10);
        }
        LOG_INF("Sent: %u, errors: %u", counter, error_counter);

        if (error_counter == 0) {
            has_changed = false;
        }
    }

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

            ret = publish_new(&mflt_evt, sizeof(mflt_evt),
                              orb_mcu_main_McuToJetson_memfault_event_tag,
                              remote);
#ifdef CONFIG_ZTEST
            // don't care about tx failures in tests
            ret = RET_SUCCESS;
#endif
            if (ret) {
                // come back later to send the same chunk
                return ret;
            }

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

    return ret;
}

ret_code_t
diag_set_status(orb_mcu_HardwareDiagnostic_Source source,
                orb_mcu_HardwareDiagnostic_Status status)
{
    if (source >= orb_mcu_HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL) {
        return RET_ERROR_INVALID_PARAM;
    }

    if (hw_statuses[source] == status) {
        return RET_SUCCESS;
    }

    hw_statuses[source] = status;
    has_changed = true;

    return RET_SUCCESS;
}

void
diag_init(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(hw_statuses); i++) {
        hw_statuses[i] = orb_mcu_HardwareDiagnostic_Status_STATUS_UNKNOWN;
    }
    has_changed = false;
}

#ifdef CONFIG_MEMFAULT
static void
collect_thread_stack_usage_cb(const struct k_thread *thread, void *user_data)
{
    UNUSED_PARAMETER(user_data);

    static const struct {
        const char *name;
        MemfaultMetricId id;
    } threads[] = {
        {"runner", MEMFAULT_METRICS_KEY(runner_stack_free_bytes)},
    };

    for (size_t i = 0; i < ARRAY_SIZE(threads); i++) {
        if (strcmp(thread->name, threads[i].name) == 0) {
            size_t unused = 0;
            int rc = k_thread_stack_space_get(thread, &unused);
            if (rc == 0) {
                memfault_metrics_heartbeat_set_unsigned(threads[i].id, unused);
            }
        }
    }
}

void
memfault_metrics_heartbeat_collect_data(void)
{
    k_thread_foreach(collect_thread_stack_usage_cb, NULL);
}
#endif

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

ZTEST(hil, test_diag)
{
    int ret;

    diag_init();
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(orb_mcu_HardwareDiagnostic_Source_UNKNOWN,
                          orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "diag_set_status() should fail");

    ret = diag_set_status(orb_mcu_HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL,
                          orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "diag_set_status() should fail");
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(orb_mcu_HardwareDiagnostic_Source_OPTICS_MIRRORS,
                          orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_SUCCESS, "diag_set_status() should succeed");
    zassert_equal(diag_has_data(), true, "diag_has_data() should be true");

    diag_sync(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(orb_mcu_HardwareDiagnostic_Source_OPTICS_MIRRORS,
                          orb_mcu_HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_SUCCESS, "diag_set_status() should succeed");
    // same status so data didn't change since sync
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");
}

#endif /* CONFIG_ZTEST */
