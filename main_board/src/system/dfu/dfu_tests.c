#include "dfu.h"
#include "orb_logs.h"
#include "runner/runner.h"
#include <can_messaging.h>
#include <flash_map_backend/flash_map_backend.h>
#include <main.pb.h>
#include <pb_encode.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(dfutest, LOG_LEVEL_DBG);

#ifdef CONFIG_BOARD_DIAMOND_MAIN
#define CRC_EXPECTED 0xe7ac67e1
#elif CONFIG_BOARD_PEARL_MAIN
#define CRC_EXPECTED 0x896f20bb
#else
#error fix the test for any other board
#endif

ZTEST(hil, test_dfu_upload_tests)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_DFU);

    // with block size of 39 bytes, we can test:
    // - erasing two times on new flash sector
    // - byte count in final buffer isn't aligned on double-word (useful when
    // flash is STM32)
    uint32_t test_block_count = (DFU_FLASH_SECTOR_SIZE / 39) + 1;

    can_message_t to_send;
    orb_mcu_McuMessage dfu_block = orb_mcu_McuMessage_init_zero;
    dfu_block.version = orb_mcu_Version_VERSION_0;
    dfu_block.which_message = orb_mcu_McuMessage_j_message_tag;
    dfu_block.message.j_message.which_payload =
        orb_mcu_main_JetsonToMcu_dfu_block_tag;

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
        bool encoded = pb_encode_ex(&stream, orb_mcu_McuMessage_fields,
                                    &dfu_block, PB_ENCODE_DELIMITED);
        to_send.size = stream.bytes_written;
        to_send.bytes = buffer;
        to_send.destination = 0;

        zassert_true(encoded);
        if (encoded) {
            int ret = runner_handle_new_can(&to_send);
            zassert_true(ret == 0);
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

        k_msleep(100);
    }

    LOG_INF("Reading back flash");

    // open Flash
    const struct flash_area *fap = NULL;
    int rc;

    rc = flash_area_open(DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)), &fap);
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

// Test CRC speed over entire secondary slot (external flash on Diamond)
ZTEST(hil, test_crc_over_flash)
{
    const struct flash_area *flash_area_p = NULL;
    int ret = flash_area_open(DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)),
                              &flash_area_p);
    zassert_equal(ret, 0, "Unable to open secondary slot");

    // create false image header to test CRC speed over entire flash area
    struct image_header secondary_slot_header = {0};
    secondary_slot_header.ih_magic = 0x12345678;
    secondary_slot_header.ih_ver.iv_major = 0x12;
    secondary_slot_header.ih_ver.iv_minor = 0x34;
    secondary_slot_header.ih_ver.iv_revision = 0x5678;
    secondary_slot_header.ih_ver.iv_build_num = 0x9abcdef0;
    secondary_slot_header.ih_hdr_size = DFU_FLASH_PAGE_SIZE;
    secondary_slot_header.ih_img_size =
        flash_area_p->fa_size - DFU_FLASH_PAGE_SIZE;

    k_msleep(10);
    ret = flash_area_erase(flash_area_p, 0, flash_area_p->fa_size);
    zassert_equal(ret, 0, "Unable to erase secondary slot");

    ret = flash_area_write(flash_area_p, 0, (uint8_t *)&secondary_slot_header,
                           sizeof(secondary_slot_header));
    zassert_equal(ret, 0, "Unable to write header into secondary slot");

    flash_area_close(flash_area_p);

    k_msleep(10);
    int64_t tick_ms = k_uptime_get();
    ret = dfu_secondary_check(CRC_EXPECTED);
    int64_t crc_computation_ms = k_uptime_delta(&tick_ms);
    zassert_equal(ret, RET_SUCCESS,
                  "Please fix CRC_EXPECTED, as it should be deterministic "
                  "(erased slot, known size)");

    LOG_INF("CRC over entire slot took %llu ms", crc_computation_ms);

#ifdef CONFIG_BOARD_DIAMOND_MAIN
    // check within 1450ms (1.45s)
    // ack timeout is 1.5s (orb-mcu-util)
    // you read that well, access to external SPI Flash takes time...
    zassert_between_inclusive(crc_computation_ms, 0, 1450);
#elif defined(CONFIG_BOARD_PEARL_MAIN)
    // check within 50ms
    zassert_between_inclusive(crc_computation_ms, 0, 50);
#else
#error fix the test for any other board
#endif
}

ZTEST(hil, test_get_versions)
{
    int ret;
    struct image_header ih = {0};
    ih.ih_ver.iv_major = 1;
    ih.ih_ver.iv_minor = 2;
    ih.ih_ver.iv_revision = 3;
    ih.ih_ver.iv_build_num = 4;

    /* initialize the version on Flash */
    const struct flash_area *flash_area_p = NULL;
    ret = flash_area_open(DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)),
                          &flash_area_p);
    zassert_equal(ret, 0, "Unable to open secondary slot");

    ret = flash_area_erase(flash_area_p, 0, flash_area_p->fa_size);
    zassert_equal(ret, 0, "Unable to erase secondary slot");

    ret = flash_area_write(flash_area_p, 0, (void *)&ih, sizeof(ih));
    zassert_equal(ret, 0, "Unable to write version to secondary slot");

    flash_area_close(flash_area_p);

    ret = dfu_version_secondary_get(NULL);
    zassert_equal(ret, RET_ERROR_INVALID_PARAM,
                  "NULL is an invalid input and must be treated like that");

    memset(&ih, 0, sizeof(ih));
    ret = dfu_version_secondary_get(&ih.ih_ver);
    zassert_equal(ret, 0, "Unable to get version from secondary slot");

    zassert_equal(ih.ih_ver.iv_major, 1, "Major version mismatch");
    zassert_equal(ih.ih_ver.iv_minor, 2, "Minor version mismatch");
    zassert_equal(ih.ih_ver.iv_revision, 3, "Revision version mismatch");
    zassert_equal(ih.ih_ver.iv_build_num, 4, "Build number mismatch");

    ret = dfu_version_primary_get(&ih.ih_ver);
    zassert_equal(ret, 0, "Unable to get version from primary slot");
}
