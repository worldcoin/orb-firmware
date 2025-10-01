#include "orb_logs.h"
#include "storage.h"
#include <errors.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(storage_tests, CONFIG_STORAGE_LOG_LEVEL);

static void
clean_storage(void *fixture)
{
    ARG_UNUSED(fixture);

    const struct flash_area *fa;

    int ret = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    if (ret == 0) {
        ret = flash_area_erase(fa, 0, fa->fa_size);
        zassert_equal(ret, 0, "flash_area_erase failed %d", ret);
        if (ret == 0) {
            ret = storage_init();
            zassert_equal(ret, 0, "storage_init failed %d", ret);
        }
    }

    if (ret != 0) {
        LOG_ERR("Unable to erase storage for successful unit tests");
    }
}

ZTEST_SUITE(storage, NULL, NULL, clean_storage, NULL, clean_storage);

ZTEST(storage, test_init_erased)
{
    int ret;
    const struct flash_area *fa;
    struct storage_area_s area;

    ret = storage_init();
    zassert_equal(ret, RET_SUCCESS, "storage_init failed %d", ret);

    // check pointers after init
    get_storage_area(&area);
    zassert_equal(area.wr_idx, area.rd_idx, "get_storage_area failed %d", ret);
    zassert_equal((off_t)area.wr_idx, 0,
                  "indexes must be at the beginning of the flash area");

    ret = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    zassert_equal(ret, 0, "flash_area_open failed %d", ret);

    // check flash is erased
    for (off_t flash_pointer = area.wr_idx; flash_pointer < area.wr_idx + 64;
         ++flash_pointer) {
        uint32_t buf;
        flash_area_read(fa, (off_t)flash_pointer, &buf, sizeof(buf));
        zassert_equal(buf, 0xFFFFFFFF, "flash must be erased");
    }
}

ZTEST(storage, test_dummy_records)
{
    struct storage_area_s area;

    int ret = storage_init();
    zassert_equal(ret, RET_SUCCESS, "storage_init failed %d", ret);

    // write tests
    LOG_INF("Writing 1 dummy record to storage area");
#if FLASH_WRITE_BLOCK_SIZE == 1
    const size_t record_size = 8;
#else
    const size_t record_size = FLASH_WRITE_BLOCK_SIZE;
#endif
    char dummy_record[record_size * 3];
    for (size_t i = 0; i < sizeof(dummy_record); ++i) {
        dummy_record[i] = rand() % UINT8_MAX;
    }
    ret = storage_push(dummy_record, sizeof(dummy_record));
    zassert_equal(ret, RET_SUCCESS, "storage_push failed %d (aligned record)",
                  ret);

    // check area after successful writing
    get_storage_area(&area);
    zassert_equal(
        area.wr_idx,
        (off_t)(area.rd_idx + sizeof(storage_header_t) + sizeof(dummy_record)),
        "write must be pushed after the last written record");

    LOG_INF(
        "Writing 1 dummy record (to be padded to block size) to storage area");
    char dummy_record_padded[record_size * 2 + 2];
    for (size_t i = 0; i < sizeof(dummy_record_padded); ++i) {
        dummy_record_padded[i] = rand() % UINT8_MAX;
    }
    off_t wr_idx_copy = area.wr_idx;
    UNUSED(wr_idx_copy); // only used when FLASH_WRITE_BLOCK_SIZE != 1

    ret = storage_push(dummy_record_padded, sizeof(dummy_record_padded));
    zassert_equal(ret, RET_SUCCESS, "storage_push failed %d (padded)", ret);

    // check area after successful writing
    get_storage_area(&area);
#if FLASH_WRITE_BLOCK_SIZE != 1
    zassert_equal((off_t)(wr_idx_copy + sizeof(storage_header_t) +
                          (FLASH_WRITE_BLOCK_SIZE * 3)),
                  area.wr_idx,
                  "write index must have moved with padding included");
    zassert_equal((off_t)(wr_idx_copy + sizeof(storage_header_t) +
                          (FLASH_WRITE_BLOCK_SIZE * 3)),
                  area.wr_idx,
                  "write index must have moved with padding included");
#endif

    // read back aligned record
    LOG_INF("Read back records, check content and free up storage");
    char read_record[record_size * 3];
    memset(read_record, 0x00, sizeof(read_record));
    size_t size = 0;
    ret = storage_peek(read_record, &size);
    zassert_equal(
        ret, RET_ERROR_NO_MEM,
        "storage_peek must fail because size is too small to fit record", ret);

    size = sizeof(read_record);
    ret = storage_peek(read_record, &size);
    zassert_equal(ret, RET_SUCCESS, "storage_peek failed %d (aligned record)",
                  ret);
    zassert_equal(size, sizeof(dummy_record),
                  "size must be size of dummy_record but is %u", size);

    for (size_t i = 0; i < sizeof(dummy_record); ++i) {
        zassert_equal(read_record[i], dummy_record[i], "Contents must match");
    }

    // free aligned record
    ret = storage_free();
    zassert_equal(ret, RET_SUCCESS);

    // read back padded record
    memset(read_record, 0x00, sizeof(read_record));
    size = sizeof(read_record);
    ret = storage_peek(read_record, &size);
    zassert_equal(ret, RET_SUCCESS, "storage_peek failed %d (padded record)",
                  ret);
    zassert_equal(size, sizeof(dummy_record_padded),
                  "size must be sizeof(dummy_record_padded)");
    for (size_t i = 0; i < sizeof(dummy_record_padded); ++i) {
        zassert_equal(read_record[i], dummy_record_padded[i],
                      "Contents must match");
    }

    get_storage_area(&area);
    zassert_not_equal((off_t)area.rd_idx, 0,
                      "read index must not be at the beginning of the area");

    // free padded record
    ret = storage_free();
    zassert_equal(ret, RET_SUCCESS);

    get_storage_area(&area);
    zassert_equal(
        area.rd_idx, area.wr_idx,
        "write and read index must be identical after freeing the only "
        "record in storage");

    LOG_INF("Add one record and re-initialize area to ensure correct "
            "initialization");
    // set initial content with 1 record not at the beginning of the area
    ret = storage_push(dummy_record, sizeof(dummy_record));
    zassert_equal(ret, RET_SUCCESS, "storage_push failed %d (aligned record)",
                  ret);

    // read back entire flash area and check pointers
    ret = storage_init();
    get_storage_area(&area);
    zassert_equal(ret, RET_SUCCESS);
    zassert_equal(
        area.wr_idx,
        (off_t)(area.rd_idx + sizeof(storage_header_t) + sizeof(dummy_record)),
        "storage_init must find the `record_aligned` record");
}

ZTEST(storage, test_free_empty)
{
    int ret;

    ret = storage_init();
    zassert_equal(ret, RET_SUCCESS, "storage_init failed %d", ret);

    // free empty storage must result in failure
    ret = storage_free();
    zassert_not_equal(ret, RET_SUCCESS);
}

ZTEST(storage, test_full_storage)
{
    int ret;

    // fill storage entirely
    LOG_INF("Fill storage entirely, start from erased content");

    ret = storage_init();
    zassert_equal(ret, RET_SUCCESS, "storage_init failed %d", ret);

#if FLASH_WRITE_BLOCK_SIZE == 1
    const size_t record_size = 8;
#else
    const size_t record_size = FLASH_WRITE_BLOCK_SIZE;
#endif
    char dummy_record[record_size * 3];
    for (size_t i = 0; i < sizeof(dummy_record); ++i) {
        dummy_record[i] = rand() % UINT8_MAX;
    }
    uint32_t count = 0;
    do {
        ret = storage_push(dummy_record, sizeof(dummy_record));
        if (ret == RET_SUCCESS) {
            ++count;
        }
    } while (ret == RET_SUCCESS);
    LOG_INF("Filled storage with %u records", count);
    zassert_equal(ret, RET_ERROR_NO_MEM,
                  "error writing records not due to area full: %d", ret);

    const struct flash_area *fa;
    ret = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    zassert_equal(ret, 0, "flash_area_open failed %d", ret);
    uint32_t record_count_expected =
        fa->fa_size / (sizeof(storage_header_t) + sizeof(dummy_record));
    zassert_equal(count, record_count_expected, "expected: %u, was: %u",
                  record_count_expected, count);

    // make sure we are able to initialize a full area
    LOG_INF("Initializing a full area");
    ret = storage_init();
    zassert_equal(ret, RET_SUCCESS);

    struct storage_area_s area;
    get_storage_area(&area);
    size_t bytes_used = (size_t)area.wr_idx - (size_t)area.rd_idx;
    zassert_equal(bytes_used, area.fa->fa_size, "area must be full now");

    for (uint32_t i = 0; i < count; ++i) {
        ret = storage_free();
        if (ret) {
            LOG_ERR("Unable to free area completely: %d", ret);
            break;
        }
    }

    get_storage_area(&area);
    zassert_equal((off_t)area.wr_idx, 0, "storage area must be reset");
    zassert_equal(area.wr_idx, area.rd_idx, "storage area should be reset");

    storage_header_t header;
    flash_area_read(fa, 0, &header, sizeof(header));

    zassert_equal(header.magic_state, RECORD_UNUSED, "area must be erased");
}
