#include "mcu_messaging.pb.h"
#include "pubsub.h"
#include <errors.h>
#include <utils.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pubsub_test);

static uint32_t mcu_to_jetson_payloads = 0;
static int error_code = 0;

void
publish_start(void)
{
    // nothing, Jetson not started in test mode
}

// redefinition of publish_new for tests
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    if (which_payload > 19) {
        error_code = RET_ERROR_INVALID_PARAM;
        return error_code;
    }

    if (size > STRUCT_MEMBER_SIZE_BYTES(McuToJetson, payload)) {
        error_code = RET_ERROR_NO_MEM;
        return error_code;
    }

    mcu_to_jetson_payloads |= (1 << which_payload);

    return RET_SUCCESS;
}

int
publish_store(void *payload, size_t size, uint32_t which_payload,
              uint32_t remote_addr)
{
    return publish_new(payload, size, which_payload, remote_addr);
}

ZTEST(runtime_tests_2, pubsub)
{
    zassert_equal(error_code, 0);

    // make sure these payloads have been reported by their respective modules
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_battery_voltage_tag), 0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_battery_capacity_tag), 0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_temperature_tag), 0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_fan_status_tag), 0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_motor_range_tag), 0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_battery_diag_tag), 0);
    zassert_not_equal(mcu_to_jetson_payloads & (1 << McuToJetson_tof_1d_tag),
                      0);
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << McuToJetson_gnss_partial_tag), 0);
    zassert_not_equal(mcu_to_jetson_payloads & (1 << McuToJetson_front_als_tag),
                      0);
}
