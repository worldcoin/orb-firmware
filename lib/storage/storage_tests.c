#include "storage_tests.h"
#include "storage.h"
#include <device.h>
#include <errors.h>
#include <logging/log.h>
#include <storage/flash_map.h>

LOG_MODULE_REGISTER(storage_tests, CONFIG_STORAGE_LOG_LEVEL);

void
storage_tests(void)
{
    int ret;
    const struct flash_area *fa;

    /// used for testing after init, read and write operations
    struct storage_area_s area;

    // start by erasing flash content
    ret = flash_area_open(FLASH_AREA_ID(storage), &fa);
    __ASSERT_NO_MSG(ret == 0);
    flash_area_erase(fa, 0, fa->fa_size);

    ret = storage_init();
    __ASSERT(ret == RET_SUCCESS, "storage_init failed %d", ret);

    // check pointers after init
    get_storage_area(&area);
    __ASSERT(area.wr_idx == area.rd_idx, "write and read index must match");
    __ASSERT((off_t)area.wr_idx == area.fa->fa_off,
             "indexes must be at the beginning of the flash area");

    // check flash is erased
    for (uint32_t *flash_pointer = (uint32_t *)area.wr_idx;
         flash_pointer < (uint32_t *)(area.wr_idx + 64); ++flash_pointer) {
        __ASSERT_NO_MSG(*flash_pointer == 0xFFFFFFFF);
    }

    // write tests
    char dummy_record[FLASH_WRITE_BLOCK_SIZE * 3] = {0xaa};
    ret = storage_push(dummy_record, sizeof(dummy_record));
    __ASSERT(ret == RET_SUCCESS, "storage_push failed %d (aligned record)",
             ret);

    // check area after successful write
    get_storage_area(&area);
    __ASSERT(area.wr_idx == (storage_header_t *)((size_t)area.rd_idx +
                                                 sizeof(storage_header_t) +
                                                 sizeof(dummy_record)),
             "write must be pushed after the last written record");

    void *wr_idx_ptr_copy = area.wr_idx;
    char dummy_record_incorrect_size[FLASH_WRITE_BLOCK_SIZE * 2 + 2] = {0x55};
    ret = storage_push(dummy_record_incorrect_size,
                       sizeof(dummy_record_incorrect_size));
    __ASSERT(ret == RET_ERROR_INVALID_PARAM,
             "storage_push on unaligned content must have failed "
             "(RET_ERROR_INVALID_PARAM) but was %d",
             ret);

    get_storage_area(&area);
    __ASSERT(wr_idx_ptr_copy == area.wr_idx, "write index must not be moved");

    // read back
    char read_record_aligned[sizeof(dummy_record)] = {0x00};
    size_t size = 0;
    ret = storage_peek(read_record_aligned, &size);
    __ASSERT(ret == RET_SUCCESS, "storage_peek failed %d (aligned record)",
             ret);
    __ASSERT(size == sizeof(dummy_record),
             "size must be FLASH_WRITE_BLOCK_SIZE*2");
    for (size_t i = 0; i < sizeof(dummy_record); ++i) {
        __ASSERT(read_record_aligned[i] == dummy_record[i],
                 "Contents must match");
    }

    // free
    ret = storage_free();
    __ASSERT_NO_MSG(ret == RET_SUCCESS);
    get_storage_area(&area);
    __ASSERT(area.rd_idx == area.wr_idx,
             "write and read index must be identical after freeing the only "
             "record in storage");

    // free empty storage must result in failure
    ret = storage_free();
    __ASSERT_NO_MSG(ret != RET_SUCCESS);

    // set initial content with 1 record not at the beginning of the area
    ret = storage_push(dummy_record, sizeof(dummy_record));
    __ASSERT(ret == RET_SUCCESS, "storage_push failed %d (aligned record)",
             ret);

    // read back entire flash area and check pointers
    ret = storage_init();
    __ASSERT_NO_MSG(ret == RET_SUCCESS);
    get_storage_area(&area);
    __ASSERT((off_t)area.rd_idx != area.fa->fa_off,
             "read index must not be at the beginning of the area");
    __ASSERT(area.wr_idx == (storage_header_t *)((size_t)area.rd_idx +
                                                 sizeof(storage_header_t) +
                                                 sizeof(dummy_record)),
             "storage_init must find the `record_aligned` record");

    // fill storage entirely
    // erase flash content
    flash_area_erase(fa, 0, fa->fa_size);
    ret = storage_init();
    __ASSERT_NO_MSG(ret == RET_SUCCESS);

    uint32_t count = 0;
    do {
        ret = storage_push(dummy_record, sizeof(dummy_record));
        if (ret == RET_SUCCESS) {
            ++count;
        }
    } while (ret == RET_SUCCESS);
    LOG_INF("Filled storage with %u records", count);
    __ASSERT(ret == RET_ERROR_NO_MEM,
             "error writing records not due to area full: %d", ret);

    uint32_t record_count_expected =
        fa->fa_size / (sizeof(storage_header_t) + sizeof(dummy_record));
    __ASSERT(count == record_count_expected, "expected: %u, was: %u",
             record_count_expected, count);

    // make sure we are able to initialize a full area
    ret = storage_init();
    __ASSERT_NO_MSG(ret == RET_SUCCESS);

    get_storage_area(&area);
    size_t bytes_used = (size_t)area.wr_idx - (size_t)area.rd_idx;
    __ASSERT(bytes_used == area.fa->fa_size, "area must be full now");

    for (uint32_t i = 0; i < count; ++i) {
        ret = storage_free();
        if (ret) {
            LOG_ERR("Unable to free area completely: %d", ret);
            break;
        }
    }

    get_storage_area(&area);
    __ASSERT((off_t)area.wr_idx == area.fa->fa_off,
             "storage area must be reset");
    __ASSERT(area.wr_idx == area.rd_idx, "storage area should be reset");
    __ASSERT(area.wr_idx->magic_state == RECORD_UNUSED, "area must be erased");
}
