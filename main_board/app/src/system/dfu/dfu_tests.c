#include "dfu.h"
#include "runner/runner.h"
#include <can_messaging.h>
#include <flash_map_backend/flash_map_backend.h>
#include <pb_encode.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(dfutest, LOG_LEVEL_DBG);

ZTEST(hil, test_dfu_upload_tests)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_DFU);

    // with block size of 39,
    // 53 blocks is a sweet spot for testing:
    // - uses two pages (erasing two times)
    // - byte count in final buffer isn't aligned on double-word
    uint32_t test_block_count = 53; // sys_rand32_get() % 50 + 50;

    can_message_t to_send;
    McuMessage dfu_block = McuMessage_init_zero;
    dfu_block.version = Version_VERSION_0;
    dfu_block.which_message = McuMessage_j_message_tag;
    dfu_block.message.j_message.which_payload = JetsonToMcu_dfu_block_tag;

    dfu_block.message.j_message.ack_number = 0;

    dfu_block.message.j_message.payload.dfu_block.block_count =
        test_block_count;
    dfu_block.message.j_message.payload.dfu_block.block_number = 0;
    dfu_block.message.j_message.payload.dfu_block.image_block.size =
        DFU_BLOCK_SIZE_MAX;
    memset(dfu_block.message.j_message.payload.dfu_block.image_block.bytes,
           dfu_block.message.j_message.payload.dfu_block.block_number + 1,
           DFU_BLOCK_SIZE_MAX);

    uint32_t block_to_send = test_block_count;
    uint32_t block_to_readback = test_block_count;

    LOG_INF("Writing %u blocks for the test", block_to_send);

    while (block_to_send--) {
        uint8_t buffer[CAN_FRAME_MAX_SIZE];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        bool encoded = pb_encode_ex(&stream, McuMessage_fields, &dfu_block,
                                    PB_ENCODE_DELIMITED);
        to_send.size = stream.bytes_written;
        to_send.bytes = buffer;
        to_send.destination = 0;

        zassert_true(encoded);
        if (encoded) {
            runner_handle_new_can(&to_send);
        } else {
            LOG_ERR("Error encoding DFU block");
            return;
        }

        // update next block
        dfu_block.message.j_message.ack_number += 1;
        dfu_block.message.j_message.payload.dfu_block.block_number += 1;
        dfu_block.message.j_message.payload.dfu_block.image_block.size =
            DFU_BLOCK_SIZE_MAX;
        memset(dfu_block.message.j_message.payload.dfu_block.image_block.bytes,
               dfu_block.message.j_message.payload.dfu_block.block_number + 1,
               DFU_BLOCK_SIZE_MAX);

        k_msleep(500);
    }

    LOG_INF("Reading back flash");

    // open Flash
    const struct flash_area *fap = NULL;
    int rc;

    rc = flash_area_open(flash_area_id_from_image_slot(1), &fap);
    zassert_equal(rc, 0);
    zassert_not_null(fap);

    uint8_t buf_compare[DFU_BLOCK_SIZE_MAX];
    uint8_t buf_read_back[DFU_BLOCK_SIZE_MAX];
    for (uint32_t i = 0; i < block_to_readback; ++i) {
        memset(buf_compare, i + 1, sizeof(buf_compare));
        memset(buf_read_back, 0, sizeof(buf_read_back));

        rc = flash_area_read(fap, i * DFU_BLOCK_SIZE_MAX, buf_read_back,
                             sizeof(buf_read_back));
        zassert_equal(rc, 0);
        if (rc != 0) {
            LOG_ERR("Test failed, error reading flash, rc %i", rc);
            break;
        }

        rc = memcmp(buf_read_back, buf_compare, sizeof(buf_read_back));
        zassert_equal(rc, 0);
        if (rc != 0) {
            LOG_ERR("Test failed, incorrect flash content (%u)", i + 1);
            rc = -EILSEQ;
            break;
        }
    }

    if (fap) {
        flash_area_close(fap);
    }

    zassert_equal(rc, 0);
}

ZTEST(hil, test_crc_over_flash)
{
    // Test CRC speed over entire slot
    uint8_t *sec_slot = (uint8_t *)0x8044000;
    uint32_t tick = k_cycle_get_32();
    crc32_ieee(sec_slot, 224 * 1024);
    uint32_t tock = k_cycle_get_32();

    uint32_t crc_computation_us =
        (tock - tick) / (sys_clock_hw_cycles_per_sec() / 1000 / 1000);
    LOG_INF("CRC over entire slot took %uus, %u cycles (%u cycle/sec)",
            crc_computation_us, tock - tick, sys_clock_hw_cycles_per_sec());
    // check within 50ms
    zassert_within(crc_computation_us, 25000, 25000);
}