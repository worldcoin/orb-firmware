#include "mcu.pb.h"
#include "pubsub.h"
#include "system/version/version.h"
#include <errors.h>
#include <utils.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "orb_logs.h"
#include <can_messaging.h>
#include <pb_encode.h>
LOG_MODULE_REGISTER(pubsub_test);

static uint32_t mcu_to_jetson_payloads = 0;
static K_SEM_DEFINE(pub_buffers_sem, 1, 1);

int
subscribe_add(uint32_t remote_addr)
{
    ARG_UNUSED(remote_addr);

    // nothing, Jetson not started in test mode
    return RET_SUCCESS;
}

bool
publish_is_started(uint32_t remote_addr)
{
    ARG_UNUSED(remote_addr);

    // allow to publish to any remote address
    return true;
}

void
publish_flush(void)
{
    // nothing to do
}

// redefinition of publish_new for tests
int
publish_new(void *payload, size_t size, uint32_t which_payload,
            uint32_t remote_addr)
{
    int err_code;

    if (which_payload > 32) {
        err_code = RET_ERROR_INVALID_PARAM;
        return err_code;
    }

    if (size > STRUCT_MEMBER_SIZE_BYTES(orb_mcu_main_McuToJetson, payload)) {
        err_code = RET_ERROR_NO_MEM;
        return err_code;
    }

    // test encoding
    k_timeout_t timeout = k_is_in_isr() ? K_NO_WAIT : K_MSEC(5);
    err_code = k_sem_take(&pub_buffers_sem, timeout);
    if (err_code) {
        goto free_on_error;
    }

    uint8_t buffer[orb_mcu_main_McuToJetson_size +
                   MCU_MESSAGE_ENCODED_WRAPPER_SIZE];
    static orb_mcu_McuMessage message = {.version = orb_mcu_Version_VERSION_0,
                                         .which_message =
                                             orb_mcu_McuMessage_m_message_tag,
                                         .message.m_message = {0}};

    message.message.m_message.which_payload = which_payload;
    memcpy(&message.message.m_message.payload, payload, size);

    // encode full orb_mcu_McuMessage
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    bool encoded = pb_encode_ex(&stream, orb_mcu_McuMessage_fields, &message,
                                PB_ENCODE_DELIMITED);

    if (!encoded) {
        LOG_ERR("Error encoding: %u, err: %s", which_payload, stream.errmsg);
        err_code = RET_ERROR_INTERNAL;
        goto free_on_error;
    }

    // check encoded data size < CAN_MAX_DLEN if message sent through CAN-FD
    if ((remote_addr & CAN_ADDR_IS_ISOTP) == 0 &&
        stream.bytes_written > CAN_MAX_DLEN) {
        LOG_ERR("Encoded payload (id %u, size %u) doesn't fit a CAN FD frame "
                "(encoded sz %u)",
                which_payload, size, stream.bytes_written);
        err_code = RET_ERROR_FORBIDDEN;
        goto free_on_error;
    }

    mcu_to_jetson_payloads |= (1 << which_payload);

free_on_error:
    k_sem_give(&pub_buffers_sem);

    return err_code;
}

int
publish_store(void *payload, size_t size, uint32_t which_payload,
              uint32_t remote_addr)
{
    return publish_new(payload, size, which_payload, remote_addr);
}

ZTEST(hil, test_pubsub_sent_messages)
{
    // make sure these payloads have been reported by their respective modules
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_battery_voltage_tag),
                      0);
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_battery_capacity_tag),
                      0);
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_temperature_tag),
                      0);
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_fan_status_tag),
                      0);
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_voltage_tag),
                      0);
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_motor_range_tag),
                      0);
    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_diag_common_tag),
        0);
#ifndef CONFIG_BOARD_DIAMOND_MAIN
    zassert_not_equal(
        mcu_to_jetson_payloads & (1 << orb_mcu_main_McuToJetson_tof_1d_tag), 0);
#endif

    // init to 0 so that the test is performed by default
    // won't be done on diamond with front-unit 6.3x
    uint32_t fu_version = 0;
#if defined(CONFIG_BOARD_DIAMOND_MAIN)
    fu_version = version_get_front_unit_rev();
#endif
    if (fu_version <
            orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3A ||
        fu_version >
            orb_mcu_Hardware_FrontUnitVersion_FRONT_UNIT_VERSION_V6_3C) {
        zassert_not_equal(mcu_to_jetson_payloads &
                              (1 << orb_mcu_main_McuToJetson_front_als_tag),
                          0);
    }

    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_hw_state_tag),
                      0);

    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_info_hw_fw_tag),
        0);
    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_info_max_values_tag),
        0);
    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_info_soc_and_statistics_tag),
        0);

#if defined(CONFIG_BOARD_PEARL_MAIN)
    zassert_not_equal(mcu_to_jetson_payloads &
                          (1 << orb_mcu_main_McuToJetson_gnss_partial_tag),
                      0);
    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_diag_safety_tag),
        0);
    zassert_not_equal(
        mcu_to_jetson_payloads &
            (1 << orb_mcu_main_McuToJetson_battery_diag_permanent_fail_tag),
        0);
#endif
}
