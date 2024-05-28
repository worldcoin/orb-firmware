#include "storage.h"
#include "orb_logs.h"
#include <errors.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(storage, CONFIG_STORAGE_LOG_LEVEL);

BUILD_ASSERT(sizeof(storage_header_t) % FLASH_WRITE_BLOCK_SIZE == 0,
             "storage_header_t must have a size aligned to "
             "FLASH_WRITE_BLOCK_SIZE bytes");

static struct storage_area_s storage_area = {0};

#define INVALID_INDEX       ((off_t)0xFFFFFFFF)
#define UNUSED_UINT16       0xFFFF
#define MINIMUM_EMPTY_SPACE 512 //!< empty space to keep before erasing flash

static K_SEM_DEFINE(sem_storage, 1, 1);

static int
init_area(const struct flash_area *fa)
{
    int ret;
    storage_area.rd_idx = INVALID_INDEX;
    storage_area.wr_idx = INVALID_INDEX;

    // the first byte must be the beginning of a record
    storage_header_t header = {0};
    ret = flash_area_read(fa, 0, (void *)&header, sizeof(storage_header_t));
    if (ret != 0 || (header.magic_state != RECORD_UNUSED &&
                     header.magic_state != RECORD_VALID &&
                     header.magic_state != RECORD_INVALID)) {
        storage_area.rd_idx = 0;
        storage_area.wr_idx = 0;

        return RET_ERROR_INVALID_STATE;
    }

    off_t index = 0;
    do {
        ret = flash_area_read(fa, index, (void *)&header,
                              sizeof(storage_header_t));
        if (ret) {
            storage_area.rd_idx = INVALID_INDEX;
            storage_area.wr_idx = INVALID_INDEX;
            break;
        }

        // rd_idx set to the first valid record that we find
        if (storage_area.rd_idx == INVALID_INDEX &&
            header.magic_state == RECORD_VALID) {
            // read record to check crc16
            uint8_t record[header.record_size];
            ret = flash_area_read(fa, (off_t)(index + sizeof(storage_header_t)),
                                  (void *)record, header.record_size);
            if (ret == 0 && header.crc16 == crc16_ccitt(0xffff, record,
                                                        header.record_size)) {
                storage_area.rd_idx = index;
            }
        }

        // wr_idx set to the first unused record that we found
        // rd_idx should be already set when hitting this condition
        // or will be set to wr_idx, discarding all data before wr_idx
        if (storage_area.wr_idx == INVALID_INDEX &&
            header.magic_state == RECORD_UNUSED) {
            storage_area.wr_idx = index;
            break;
        }

        if (header.magic_state == RECORD_VALID) {
            // record_size can be used to walk through the flash area
            // but storage_header_t must be block-aligned so add padding
            // if some were added
            size_t padding = 0;
            if (header.record_size % FLASH_WRITE_BLOCK_SIZE) {
                padding = FLASH_WRITE_BLOCK_SIZE -
                          (header.record_size % FLASH_WRITE_BLOCK_SIZE);
            }
            index = (off_t)(index + sizeof(storage_header_t) +
                            header.record_size + padding);
        } else {
            // try to find next valid record which must be aligned on
            // FLASH_WRITE_BLOCK_SIZE
            index = index + FLASH_WRITE_BLOCK_SIZE;
        }
    } while (index + sizeof(storage_header_t) < fa->fa_size);

    if (storage_area.rd_idx == INVALID_INDEX &&
        storage_area.wr_idx == INVALID_INDEX) {
        storage_area.rd_idx = 0;
        storage_area.wr_idx = 0;

        return RET_ERROR_INVALID_STATE;
    }

    // no unused flash found, storage is full
    if (storage_area.rd_idx != INVALID_INDEX &&
        storage_area.wr_idx == INVALID_INDEX) {
        storage_area.wr_idx = (off_t)fa->fa_size;
    }

    // rd_idx set to wr_idx if no valid data found
    if (storage_area.rd_idx == INVALID_INDEX &&
        storage_area.wr_idx != INVALID_INDEX) {
        storage_area.rd_idx = storage_area.wr_idx;
    }

    return RET_SUCCESS;
}

static void
reset_area(const struct flash_area *fa)
{
    flash_area_erase(fa, 0, fa->fa_size);
    init_area(fa);
}

int
storage_peek(char *buffer, size_t *size)
{
    int ret = RET_SUCCESS;
    k_sem_take(&sem_storage, K_FOREVER);

    if (storage_area.fa == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    // verify storage is not empty
    if (storage_area.rd_idx == storage_area.wr_idx) {
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // read header
    storage_header_t header = {0};
    ret = flash_area_read(storage_area.fa, storage_area.rd_idx, (void *)&header,
                          sizeof(storage_header_t));

    // verify `record` can hold the next record to read
    if (ret || *size < header.record_size) {
        ret = RET_ERROR_NO_MEM;
        goto exit;
    }

    // read record
    ret =
        flash_area_read(storage_area.fa,
                        (off_t)(storage_area.rd_idx + sizeof(storage_header_t)),
                        (void *)buffer, header.record_size);
    // verify record is valid with correct CRC
    if (ret || header.magic_state != RECORD_VALID ||
        (header.crc16 !=
         crc16_ccitt(0xffff, (const uint8_t *)buffer, header.record_size))) {
        ret = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    *size = header.record_size;

exit:
    k_sem_give(&sem_storage);

    return ret;
}

int
storage_free(void)
{
    int ret = RET_SUCCESS;

    k_sem_take(&sem_storage, K_FOREVER);

    if (storage_area.fa == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    storage_header_t header = {0};
    flash_area_read(storage_area.fa, storage_area.rd_idx, (void *)&header,
                    sizeof(storage_header_t));
    if (header.magic_state != RECORD_VALID) {
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // keep a copy of the freed record size before invalidating it
    uint16_t record_size = header.record_size;

    // overwrite header data with zeros, marking the data as invalid
    memset(&header, 0, sizeof(header));
    ret = flash_area_write(storage_area.fa, (off_t)storage_area.rd_idx,
                           (const void *)&header, sizeof(header));
    if (ret) {
        LOG_ERR("Unable to invalidate record: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // push read index
    size_t padding = 0;
    if (record_size % FLASH_WRITE_BLOCK_SIZE) {
        padding =
            FLASH_WRITE_BLOCK_SIZE - (record_size % FLASH_WRITE_BLOCK_SIZE);
    }
    storage_area.rd_idx =
        (off_t)(storage_area.rd_idx + sizeof(storage_header_t) + record_size +
                padding);

    LOG_DBG("New record freed, size: %u, rd off: 0x%x, wr off: 0x%x",
            record_size, (uint32_t)storage_area.rd_idx,
            (uint32_t)storage_area.wr_idx);

    if (storage_area.rd_idx >= storage_area.wr_idx) {
        size_t space_left_bytes =
            storage_area.fa->fa_size - (size_t)storage_area.wr_idx;
        if (space_left_bytes < MINIMUM_EMPTY_SPACE) {
            LOG_INF("%u bytes left, erasing", space_left_bytes);
            reset_area(storage_area.fa);
        }
    }

exit:
    k_sem_give(&sem_storage);

    return ret;
}

int
storage_push(char *record, size_t size)
{
    int ret = RET_SUCCESS;
    uint8_t padding[FLASH_WRITE_BLOCK_SIZE];
    size_t size_in_flash = size;
    size_t size_to_write_trunc = size;

    memset(padding, 0xff, sizeof(padding));

    // compute CRC16 over the record
    uint16_t crc = crc16_ccitt(0xffff, record, size);

    // size must be a multiple of FLASH_WRITE_BLOCK_SIZE
    // record will be padded with 0xff in case not
    // CRC computed over entire record in Flash, including padding
    if (size % FLASH_WRITE_BLOCK_SIZE) {
        // actual size in flash
        size_in_flash +=
            (FLASH_WRITE_BLOCK_SIZE - (size % FLASH_WRITE_BLOCK_SIZE));
        size_to_write_trunc = size - (size % FLASH_WRITE_BLOCK_SIZE);

        // copy end of the record into temporary buffer with padding included
        for (size_t i = 0; i < (size % FLASH_WRITE_BLOCK_SIZE); ++i) {
            padding[i] = record[size_to_write_trunc + i];
        }
    }

    k_sem_take(&sem_storage, K_FOREVER);

    if (storage_area.fa == NULL) {
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (((uint32_t)storage_area.wr_idx + size_in_flash) >
        storage_area.fa->fa_size) {
        ret = RET_ERROR_NO_MEM;
        goto exit;
    }

    storage_header_t header = {.magic_state = RECORD_VALID,
                               .record_size = size,
                               .crc16 = crc,
                               .unused = UNUSED_UINT16};

    // write blocks
    ret = flash_area_write(storage_area.fa,
                           (off_t)(storage_area.wr_idx + sizeof(header)),
                           (const void *)record, size_to_write_trunc);
    if (ret) {
        reset_area(storage_area.fa);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // append end of the record if not block-aligned
    if (size % FLASH_WRITE_BLOCK_SIZE) {
        ret = flash_area_write(
            storage_area.fa,
            (off_t)(storage_area.wr_idx + sizeof(header) + size_to_write_trunc),
            (const void *)padding, sizeof(padding));
        if (ret) {
            reset_area(storage_area.fa);
            ret = RET_ERROR_INTERNAL;
            goto exit;
        }
    }

    // read back content into `record` to verify it has been written correctly
    memset(record, 0x00, size);
    ret = flash_area_read(storage_area.fa,
                          (off_t)(storage_area.wr_idx + sizeof(header)),
                          (void *)record, size_to_write_trunc);
    if (ret) {
        LOG_ERR("Unable to read back record after write: %d", ret);
    }

    if (size % FLASH_WRITE_BLOCK_SIZE) {
        flash_area_read(
            storage_area.fa,
            (off_t)(storage_area.wr_idx + sizeof(header) + size_to_write_trunc),
            (void *)&record[size_to_write_trunc],
            size % FLASH_WRITE_BLOCK_SIZE);
    }

    uint16_t rb_crc = crc16_ccitt(0xffff, record, size);
    if (header.crc16 != rb_crc) {
        LOG_ERR("Invalid CRC16 read after record has been written");
        reset_area(storage_area.fa);

        ret = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    // write header
    ret = flash_area_write(storage_area.fa, storage_area.wr_idx,
                           (const void *)&header, sizeof(header));
    if (ret) {
        reset_area(storage_area.fa);

        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // push write index with padding included
    storage_area.wr_idx =
        (off_t)(storage_area.wr_idx + (sizeof(header) + size_in_flash));

    LOG_DBG("New record written, size: %u, rd off: 0x%x, wr off: 0x%x", size,
            (uint32_t)storage_area.rd_idx, (uint32_t)storage_area.wr_idx);
exit:
    k_sem_give(&sem_storage);

    return ret;
}

bool
storage_has_data(void)
{
    bool has_data;

    k_sem_take(&sem_storage, K_FOREVER);

    if (storage_area.fa == NULL) {
        k_sem_give(&sem_storage);
        LOG_ERR("Cannot check storage, not initialized");
        return false;
    }

    has_data = (storage_area.rd_idx != storage_area.wr_idx);
    k_sem_give(&sem_storage);

    return has_data;
}

int
storage_init(void)
{
    int ret;

    k_sem_take(&sem_storage, K_FOREVER);

    // reset storage area content
    memset(&storage_area, 0, sizeof(storage_area));

    ret = flash_area_open(FIXED_PARTITION_ID(storage_partition),
                          &storage_area.fa);
    if (ret) {
        LOG_ERR("Unable to open flash area: %d", ret);
        ret = RET_ERROR_NOT_INITIALIZED;
        goto exit;
    }

    ret = init_area(storage_area.fa);
    if (ret != RET_SUCCESS) {
        LOG_WRN("Unable to find valid records, erasing area");
        ret = flash_area_erase(storage_area.fa, 0, storage_area.fa->fa_size);
        if (ret) {
            LOG_ERR("Unable to erase flash area: %d", ret);
            ret = RET_ERROR_INTERNAL;
            goto exit;
        }
    }

    LOG_INF("Storage initialized: rd: 0x%x, wr: 0x%x",
            (uint32_t)storage_area.rd_idx, (uint32_t)storage_area.wr_idx);

exit:
    if (storage_area.fa != NULL) {
        flash_area_close(storage_area.fa);
    }

    k_sem_give(&sem_storage);

    return ret;
}

#if CONFIG_ORB_LIB_STORAGE_TESTS
void
get_storage_area(struct storage_area_s *area)
{
    memcpy((void *)area, (void *)&storage_area, sizeof(storage_area));
}
#endif
