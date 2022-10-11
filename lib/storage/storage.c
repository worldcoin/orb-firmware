#include "storage.h"
#include <device.h>
#include <errors.h>
#include <logging/log.h>
#include <storage/flash_map.h>
#include <sys/crc.h>

LOG_MODULE_REGISTER(storage, CONFIG_STORAGE_LOG_LEVEL);

static_assert(sizeof(storage_header_t) % FLASH_WRITE_BLOCK_SIZE == 0,
              "storage_header_t must have a size aligned to "
              "FLASH_WRITE_BLOCK_SIZE bytes");

static struct storage_area_s storage_area = {0};

#define INVALID_INDEX       ((storage_header_t *)0xFFFFFFFF)
#define UNUSED_UINT16       0xFFFF
#define MINIMUM_EMPTY_SPACE 512 //!< empty space to keep before erasing flash

K_SEM_DEFINE(sem_storage, 1, 1);

static int
init_area(const struct flash_area *fa)
{
    storage_area.rd_idx = INVALID_INDEX;
    storage_area.wr_idx = INVALID_INDEX;

    // the first byte must be the beginning of a record
    if (((storage_header_t *)fa->fa_off)->magic_state != RECORD_UNUSED &&
        ((storage_header_t *)fa->fa_off)->magic_state != RECORD_VALID &&
        ((storage_header_t *)fa->fa_off)->magic_state != RECORD_INVALID) {

        storage_area.rd_idx = (storage_header_t *)fa->fa_off;
        storage_area.wr_idx = (storage_header_t *)fa->fa_off;

        return RET_ERROR_INVALID_STATE;
    }

    for (storage_header_t *hdr = (storage_header_t *)fa->fa_off;
         (size_t)hdr < (fa->fa_off + fa->fa_size);) {

        // rd_idx set to the first valid record that we find
        if (storage_area.rd_idx == INVALID_INDEX &&
            hdr->magic_state == RECORD_VALID &&
            hdr->crc16 ==
                crc16_ccitt(0xffff,
                            (const uint8_t *)hdr + sizeof(storage_header_t),
                            hdr->record_size)) {
            storage_area.rd_idx = hdr;
        }

        // wr_idx set to the first unused record that we found
        // rd_idx should be already set when hitting this condition
        // or will be set to wr_idx, discarding all data before wr_idx
        if (storage_area.wr_idx == INVALID_INDEX &&
            hdr->magic_state == RECORD_UNUSED) {
            storage_area.wr_idx = hdr;
            break;
        }

        if (hdr->magic_state == RECORD_VALID) {
            // record_size can be used to walk through the flash area
            hdr = (storage_header_t *)((size_t)hdr + hdr->record_size +
                                       sizeof(storage_header_t));
        } else {
            // try to find next valid record which must be aligned on
            // FLASH_WRITE_BLOCK_SIZE
            hdr = (storage_header_t *)((size_t)hdr + FLASH_WRITE_BLOCK_SIZE);
        }
    }

    if (storage_area.rd_idx == INVALID_INDEX &&
        storage_area.wr_idx == INVALID_INDEX) {

        storage_area.rd_idx = (storage_header_t *)fa->fa_off;
        storage_area.wr_idx = (storage_header_t *)fa->fa_off;

        return RET_ERROR_INVALID_STATE;
    }

    // no unused flash found, storage is full
    if (storage_area.rd_idx != INVALID_INDEX &&
        storage_area.wr_idx == INVALID_INDEX) {
        storage_area.wr_idx = (storage_header_t *)(fa->fa_off + fa->fa_size);
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
storage_peek(char *record, size_t *size)
{
    int ret = RET_SUCCESS;
    k_sem_take(&sem_storage, K_FOREVER);

    // verify storage is not empty
    if (storage_area.rd_idx == storage_area.wr_idx) {
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // verify record is valid with correct CRC
    if (storage_area.rd_idx->magic_state != RECORD_VALID ||
        (storage_area.rd_idx->crc16 !=
         crc16_ccitt(0xffff,
                     (const uint8_t *)storage_area.rd_idx +
                         sizeof(storage_header_t),
                     storage_area.rd_idx->record_size))) {
        ret = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    memcpy(record,
           (void *)((size_t)storage_area.rd_idx + sizeof(storage_header_t)),
           storage_area.rd_idx->record_size);

    *size = storage_area.rd_idx->record_size;

exit:
    k_sem_give(&sem_storage);

    return ret;
}

int
storage_free(void)
{
    int ret = RET_SUCCESS;

    k_sem_take(&sem_storage, K_FOREVER);

    if (storage_area.rd_idx->magic_state != RECORD_VALID) {
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    // keep a copy of the freed record size before invalidating it
    uint16_t record_size = storage_area.rd_idx->record_size;

    // overwrite header data with zeros, marking the data as invalid
    storage_header_t header = {0};
    ret = flash_area_write(storage_area.fa,
                           (off_t)storage_area.rd_idx - storage_area.fa->fa_off,
                           (const void *)&header, sizeof(header));
    if (ret) {
        LOG_ERR("Unable to invalidate record: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // push read index
    storage_area.rd_idx =
        (storage_header_t *)((size_t)storage_area.rd_idx +
                             sizeof(storage_header_t) + record_size);

    if (storage_area.rd_idx >= storage_area.wr_idx) {
        size_t space_left_bytes =
            (storage_area.fa->fa_off + storage_area.fa->fa_size) -
            (size_t)storage_area.wr_idx;
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

    k_sem_take(&sem_storage, K_FOREVER);

    if (((uint32_t)storage_area.wr_idx + size) >
        (storage_area.fa->fa_off + storage_area.fa->fa_size)) {
        ret = RET_ERROR_NO_MEM;
        goto exit;
    }

    if (size % FLASH_WRITE_BLOCK_SIZE != 0) {
        ret = RET_ERROR_INVALID_PARAM;
        goto exit;
    }

    // fill header
    // compute CRC16
    storage_header_t header = {.magic_state = RECORD_VALID,
                               .record_size = size,
                               .crc16 = crc16_ccitt(0xffff, record, size),
                               .unused = UNUSED_UINT16};

    // write block
    ret = flash_area_write(storage_area.fa,
                           (off_t)((off_t)storage_area.wr_idx -
                                   storage_area.fa->fa_off + sizeof(header)),
                           (const void *)record, size);
    if (ret) {
        reset_area(storage_area.fa);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // verify flash is correctly written
    if (header.crc16 !=
        crc16_ccitt(0xffff,
                    (const uint8_t *)storage_area.wr_idx + sizeof(header),
                    size)) {
        LOG_ERR("Invalid CRC16 read after record has been written");
        reset_area(storage_area.fa);

        ret = RET_ERROR_INVALID_STATE;
        goto exit;
    }

    // write header
    ret = flash_area_write(storage_area.fa,
                           (off_t)storage_area.wr_idx - storage_area.fa->fa_off,
                           (const void *)&header, sizeof(header));
    if (ret) {
        reset_area(storage_area.fa);

        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // push write index
    storage_area.wr_idx = (storage_header_t *)((size_t)storage_area.wr_idx +
                                               (sizeof(header) + size));

exit:
    k_sem_give(&sem_storage);

    return ret;
}

int
storage_init(void)
{
    int ret;

    k_sem_take(&sem_storage, K_FOREVER);

    ret = flash_area_open(FLASH_AREA_ID(storage), &storage_area.fa);
    if (ret) {
        LOG_ERR("Unable to open flash area: %d", ret);
        ret = RET_ERROR_NOT_INITIALIZED;
        goto exit;
    }

    ret = init_area(storage_area.fa);
    if (ret == RET_ERROR_INVALID_STATE) {
        LOG_WRN("Unable to find valid records, erasing area");
        flash_area_erase(storage_area.fa, 0, storage_area.fa->fa_size);
        ret = RET_SUCCESS;
    }

    LOG_INF("Storage initialized: rd: 0x%x, wr: 0x%x",
            (uint32_t)storage_area.rd_idx, (uint32_t)storage_area.wr_idx);

exit:
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
