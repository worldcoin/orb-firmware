#include "diag.h"
#include "pubsub/pubsub.h"
#include <app_assert.h>
#include <errors.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(diag);

static HardwareDiagnostic_Status
    hw_statuses[HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL];

static bool has_changed;

bool
diag_has_data(void)
{
    return has_changed;
}

ret_code_t
diag_sync(uint32_t remote)
{
    int ret = RET_SUCCESS;
    size_t counter = 0;
    size_t error_counter = 0;
    HardwareDiagnostic hw_diag = HardwareDiagnostic_init_zero;

    if (has_changed == false) {
        return RET_SUCCESS;
    }

    LOG_INF("Sending statuses");

    for (size_t i = 0; i < ARRAY_SIZE(hw_statuses); i++) {
        if (hw_statuses[i] == HardwareDiagnostic_Status_STATUS_UNKNOWN) {
            continue;
        }
        hw_diag.source = i;
        hw_diag.status = hw_statuses[i];

        ret = publish_new(&hw_diag, sizeof(hw_diag),
                          McuToJetson_hardware_diag_tag, remote);
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

    return ret;
}

ret_code_t
diag_set_status(HardwareDiagnostic_Source source,
                HardwareDiagnostic_Status status)
{
    if (source >= HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL) {
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
        hw_statuses[i] = HardwareDiagnostic_Status_STATUS_UNKNOWN;
    }
    has_changed = false;
}

#ifdef CONFIG_ZTEST
#include <zephyr/ztest.h>

ZTEST(hil, test_diag)
{
    int ret;

    diag_init();
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(HardwareDiagnostic_Source_UNKNOWN,
                          HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "diag_set_status() should fail");

    ret = diag_set_status(HardwareDiagnostic_Source_MAIN_BOARD_SENTINEL,
                          HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "diag_set_status() should fail");
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(HardwareDiagnostic_Source_OPTICS_MIRRORS,
                          HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_SUCCESS, "diag_set_status() should succeed");
    zassert_equal(diag_has_data(), true, "diag_has_data() should be true");

    diag_sync(CONFIG_CAN_ADDRESS_DEFAULT_REMOTE);
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");

    ret = diag_set_status(HardwareDiagnostic_Source_OPTICS_MIRRORS,
                          HardwareDiagnostic_Status_STATUS_OK);
    zassert_equal(ret, RET_SUCCESS, "diag_set_status() should succeed");
    // same status so data didn't change since sync
    zassert_equal(diag_has_data(), false, "diag_has_data() should be false");
}

#endif /* CONFIG_ZTEST */
