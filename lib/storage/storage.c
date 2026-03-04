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

#define INVALID_INDEX       ((off_t)0xFFFFFFFF)
#define UNUSED_UINT16       0xFFFF
#define MINIMUM_EMPTY_SPACE 512 //!< empty space to keep before erasing flash

#define INIT_AREA_READ_BUFFER_SIZE 64

static K_SEM_DEFINE(sem_storage, 1, 1);

static int
init_area(struct storage_area_s *area)
{
    int ret;
    const struct flash_area *fa = area->fa;

    area->rd_idx = INVALID_INDEX;
    area->wr_idx = INVALID_INDEX;

    // the first byte must be the beginning of a record
    storage_header_t header = {0};
    ret = flash_area_read(fa, 0, (void *)&header, sizeof(storage_header_t));
    if (ret != 0 || (header.magic_state != RECORD_UNUSED &&
                     header.magic_state != RECORD_VALID &&
                     header.magic_state != RECORD_INVALID)) {
        area->rd_idx = 0;
        area->wr_idx = 0;

        return RET_ERROR_INVALID_STATE;
    }

    off_t index = 0;
    do {
        ret = flash_area_read(fa, index, (void *)&header,
                              sizeof(storage_header_t));
        if (ret) {
            area->rd_idx = INVALID_INDEX;
            area->wr_idx = INVALID_INDEX;
            break;
        }

        // rd_idx set to the first valid record that we find
        if (area->rd_idx == INVALID_INDEX &&
            header.magic_state == RECORD_VALID) {
            // read record with chunks and compute crc to check if
            // the record is valid
            uint8_t read_buffer[INIT_AREA_READ_BUFFER_SIZE];
            uint16_t crc16 = 0xffff;
            for (size_t i = 0; i < header.record_size;
                 i += INIT_AREA_READ_BUFFER_SIZE) {
                size_t size_to_read =
                    i + INIT_AREA_READ_BUFFER_SIZE < header.record_size
                        ? INIT_AREA_READ_BUFFER_SIZE
                        : header.record_size - i;
                ret = flash_area_read(
                    fa, (off_t)(index + sizeof(storage_header_t) + i),
                    (void *)read_buffer, size_to_read);
                if (ret == 0) {
                    crc16 = crc16_ccitt(crc16, read_buffer, size_to_read);
                }
            }
            if (header.crc16 == crc16) {
                area->rd_idx = index;
            }
        }

        // wr_idx set to the first unused record that we found
        // rd_idx should be already set when hitting this condition
        // or will be set to wr_idx, discarding all data before wr_idx
        if (area->wr_idx == INVALID_INDEX &&
            header.magic_state == RECORD_UNUSED) {
            area->wr_idx = index;
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

    if (area->rd_idx == INVALID_INDEX && area->wr_idx == INVALID_INDEX) {
        area->rd_idx = 0;
        area->wr_idx = 0;

        return RET_ERROR_INVALID_STATE;
    }

    // no unused flash found, storage is full
    if (area->rd_idx != INVALID_INDEX && area->wr_idx == INVALID_INDEX) {
        area->wr_idx = (off_t)fa->fa_size;
    }

    // rd_idx set to wr_idx if no valid data found
    if (area->rd_idx == INVALID_INDEX && area->wr_idx != INVALID_INDEX) {
        area->rd_idx = area->wr_idx;
    }

    LOG_DBG("Storage area initialized: rd: 0x%04lx, wr: 0x%04lx", area->rd_idx,
            area->wr_idx);

    return RET_SUCCESS;
}

static void
reset_area(struct storage_area_s *area)
{
    flash_area_erase(area->fa, 0, area->fa->fa_size);
    init_area(area);
}

int
storage_peek(struct storage_area_s *area, char *buffer, size_t *size)
{
    int flash_read_ret = 0;
    int err_code = RET_SUCCESS;
    k_sem_take(&sem_storage, K_FOREVER);

    if (area->fa == NULL) {
        err_code = RET_ERROR_NOT_INITIALIZED;
        goto exit;
    }

    // verify storage is not empty
    if (area->rd_idx == area->wr_idx) {
        err_code = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // read header
    storage_header_t header = {0};
    flash_read_ret = flash_area_read(area->fa, area->rd_idx, (void *)&header,
                                     sizeof(storage_header_t));
    if (flash_read_ret) {
        goto exit;
    } else if (*size < header.record_size) {
        err_code = RET_ERROR_NO_MEM;
        goto exit;
    }

    // read record
    flash_read_ret = flash_area_read(
        area->fa, (off_t)(area->rd_idx + sizeof(storage_header_t)),
        (void *)buffer, header.record_size);
    if (flash_read_ret) {
        goto exit;
    } else if (header.magic_state != RECORD_VALID ||
               (header.crc16 != crc16_ccitt(0xffff, (const uint8_t *)buffer,
                                            header.record_size))) {
        err_code = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    *size = header.record_size;

exit:
    if (flash_read_ret || err_code == RET_ERROR_INVALID_STATE) {
        const uint32_t rd_idx = area->rd_idx;
        const uint32_t wr_idx = area->wr_idx;
        reset_area(area);
        // print log after area reset to keep it in flash in case remote not
        // accessible
        LOG_ERR("Area in invalid state has been reset (flash read ret %d); rd: "
                "0x%04x, wr: 0x%04x",
                flash_read_ret, rd_idx, wr_idx);
        err_code = RET_ERROR_INVALID_STATE;
    }

    k_sem_give(&sem_storage);

    return err_code;
}

int
storage_free(struct storage_area_s *area)
{
    int ret = RET_SUCCESS;

    k_sem_take(&sem_storage, K_FOREVER);

    if (area->fa == NULL) {
        ret = RET_ERROR_NOT_INITIALIZED;
        goto exit;
    }

    storage_header_t header = {0};
    flash_area_read(area->fa, area->rd_idx, (void *)&header,
                    sizeof(storage_header_t));
    if (header.magic_state != RECORD_VALID) {
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // keep a copy of the freed record size before invalidating it
    uint16_t record_size = header.record_size;

    // overwrite header data with zeros, marking the data as invalid
    memset(&header, 0, sizeof(header));
    ret = flash_area_write(area->fa, (off_t)area->rd_idx, (const void *)&header,
                           sizeof(header));
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
    area->rd_idx = (off_t)(area->rd_idx + sizeof(storage_header_t) +
                           record_size + padding);

    LOG_DBG("New record freed, size: %u, rd off: 0x%x, wr off: 0x%x",
            record_size, (uint32_t)area->rd_idx, (uint32_t)area->wr_idx);

    if (area->rd_idx >= area->wr_idx) {
        size_t space_left_bytes = area->fa->fa_size - (size_t)area->wr_idx;
        if (space_left_bytes < MINIMUM_EMPTY_SPACE) {
            LOG_INF("%u bytes left, erasing", space_left_bytes);
            reset_area(area);
        }
    }

exit:
    k_sem_give(&sem_storage);

    return ret;
}

int
storage_push(struct storage_area_s *area, char *record, size_t size)
{
    int ret;

    if (record == NULL || size == 0) {
        return RET_ERROR_INVALID_PARAM;
    }

    ret = k_sem_take(&sem_storage, K_FOREVER);
    if (ret == 0) {
        if (area->fa == NULL) {
            ret = RET_ERROR_NOT_INITIALIZED;
            goto exit;
        }
        if (size > area->fa->fa_size) {
            ret = RET_ERROR_INVALID_PARAM;
            goto exit;
        }
        k_sem_give(&sem_storage);
    }

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

    if ((uint32_t)area->wr_idx + sizeof(storage_header_t) + size_in_flash >
        area->fa->fa_size) {
        ret = RET_ERROR_NO_MEM;
        goto exit;
    }

    storage_header_t header = {.magic_state = RECORD_VALID,
                               .record_size = size,
                               .crc16 = crc,
                               .unused = UNUSED_UINT16};

    // write blocks
    ret = flash_area_write(area->fa, (off_t)(area->wr_idx + sizeof(header)),
                           (const void *)record, size_to_write_trunc);
    if (ret) {
        reset_area(area);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // append end of the record if not block-aligned
    if (size % FLASH_WRITE_BLOCK_SIZE) {
        ret = flash_area_write(
            area->fa,
            (off_t)(area->wr_idx + sizeof(header) + size_to_write_trunc),
            (const void *)padding, sizeof(padding));
        if (ret) {
            reset_area(area);
            ret = RET_ERROR_INTERNAL;
            goto exit;
        }
    }

    // read back content into `record` to verify it has been written correctly
    memset(record, 0x00, size);
    ret = flash_area_read(area->fa, (off_t)(area->wr_idx + sizeof(header)),
                          (void *)record, size_to_write_trunc);
    if (ret) {
        LOG_ERR("Unable to read back record after write: %d", ret);
    }

    if (size % FLASH_WRITE_BLOCK_SIZE) {
        flash_area_read(
            area->fa,
            (off_t)(area->wr_idx + sizeof(header) + size_to_write_trunc),
            (void *)&record[size_to_write_trunc],
            size % FLASH_WRITE_BLOCK_SIZE);
    }

    uint16_t rb_crc = crc16_ccitt(0xffff, record, size);
    if (header.crc16 != rb_crc) {
        reset_area(area);
        LOG_ERR("Invalid CRC16 read after record has been written");

        ret = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    // write header
    ret = flash_area_write(area->fa, area->wr_idx, (const void *)&header,
                           sizeof(header));
    if (ret) {
        reset_area(area);

        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // push write index with padding included
    area->wr_idx = (off_t)(area->wr_idx + (sizeof(header) + size_in_flash));

    LOG_DBG("New record written, size: %u, rd off: 0x%x, wr off: 0x%x", size,
            (uint32_t)area->rd_idx, (uint32_t)area->wr_idx);
exit:
    k_sem_give(&sem_storage);

    return ret;
}

bool
storage_is_last_record(struct storage_area_s *area)
{
    bool is_last = false;

    k_sem_take(&sem_storage, K_FOREVER);

    if (area->fa == NULL || area->rd_idx == area->wr_idx) {
        /* not initialized or empty — no record at all */
        goto exit;
    }

    storage_header_t header = {0};
    int ret = flash_area_read(area->fa, area->rd_idx, (void *)&header,
                              sizeof(storage_header_t));
    if (ret || header.magic_state != RECORD_VALID) {
        goto exit;
    }

    size_t padding = 0;
    if (header.record_size % FLASH_WRITE_BLOCK_SIZE) {
        padding = FLASH_WRITE_BLOCK_SIZE -
                  (header.record_size % FLASH_WRITE_BLOCK_SIZE);
    }
    off_t next_idx = (off_t)(area->rd_idx + sizeof(storage_header_t) +
                             header.record_size + padding);

    is_last = (next_idx >= area->wr_idx);

exit:
    k_sem_give(&sem_storage);
    return is_last;
}

bool
storage_has_data(struct storage_area_s *area)
{
    bool has_data;

    k_sem_take(&sem_storage, K_FOREVER);

    if (area->fa == NULL) {
        k_sem_give(&sem_storage);
        LOG_ERR("Cannot check storage, not initialized");
        return false;
    }

    has_data = (area->rd_idx != area->wr_idx);
    k_sem_give(&sem_storage);

    return has_data;
}

int
storage_init(struct storage_area_s *area, uint8_t partition_id)
{
    int ret;

    k_sem_take(&sem_storage, K_FOREVER);

    // reset storage area content
    memset(area, 0, sizeof(*area));

    ret = flash_area_open(partition_id, &area->fa);
    if (ret) {
        LOG_ERR("Unable to open flash area (partition %u): %d", partition_id,
                ret);
        ret = RET_ERROR_NOT_INITIALIZED;
        goto exit;
    }

    ret = init_area(area);
    if (ret != RET_SUCCESS) {
        LOG_WRN("Unable to find valid records, erasing area");
        ret = flash_area_erase(area->fa, 0, area->fa->fa_size);
        if (ret) {
            LOG_ERR("Unable to erase flash area: %d", ret);
            ret = RET_ERROR_INTERNAL;
            goto exit;
        }
    }

    LOG_INF("Storage initialized (partition %u): rd: 0x%x, wr: 0x%x",
            partition_id, (uint32_t)area->rd_idx, (uint32_t)area->wr_idx);

exit:
    k_sem_give(&sem_storage);

    return ret;
}

#if CONFIG_ORB_LIB_STORAGE_TESTS
void
get_storage_area(struct storage_area_s *area, struct storage_area_s *out)
{
    memcpy((void *)out, (void *)area, sizeof(*area));
}
#endif
