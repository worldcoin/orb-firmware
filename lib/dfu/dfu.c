#include "dfu.h"
#include "bootutil/bootutil_public.h"
#include "compilers.h"
#include "errno.h"
#include <errors.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(dfu, CONFIG_DFU_LOG_LEVEL);

static void
process_dfu_blocks_thread();

K_THREAD_STACK_DEFINE(dfu_thread_stack, CONFIG_ORB_LIB_THREAD_STACK_SIZE_DFU);
static struct k_thread dfu_thread_data = {0};
static k_tid_t tid_dfu = NULL;

static struct image_header primary_slot_header = {0};
static struct image_header secondary_slot_header = {0};
// protect access to headers above
static K_SEM_DEFINE(sem_headers, 1, 1);

// Image data comes in at chunks of exactly DFU_BLOCK_SIZE_MAX, except for
// perhaps the last chunk, which can be smaller.
// The module waits to have at least DFU_BLOCKS_WRITE_SIZE bytes
// before writing into Flash. While waiting, bytes are copied into an internal
// buffer of DFU_BLOCKS_BUFFER_MIN_SIZE.
// In the case that DFU_BLOCKS_WRITE_SIZE - 1 bytes are in the block buffer
// we need at least enough space to write one more DFU_BLOCK_SIZE_MAX chunk of
// bytes to the buffer, so we end up with:
// DFU_BLOCKS_BUFFER_MIN_SIZE
//  = DFU_BLOCKS_WRITE_SIZE - 1 + DFU_BLOCK_SIZE_MAX
#define DFU_BLOCKS_WRITE_SIZE 64 /** Size of blocks written on Flash */
#define DFU_BLOCKS_BUFFER_MIN_SIZE                                             \
    (DFU_BLOCKS_WRITE_SIZE - 1 +                                               \
     DFU_BLOCK_SIZE_MAX) /** Buffer for incoming image blocks before bytes are \
                            written on Flash using DFU_BLOCKS_WRITE_SIZE-long  \
                            blocks. */

// make sure the DFU blocks buffer has a size multiple of double-word
// otherwise we won't be able to write the full buffer on Flash
#define DFU_BLOCKS_BUFFER_SIZE                                                 \
    (DFU_BLOCKS_BUFFER_MIN_SIZE + 8 - (DFU_BLOCKS_BUFFER_MIN_SIZE % 8))

BUILD_ASSERT(DFU_BLOCKS_BUFFER_SIZE % 8 == 0,
             "DFU_BLOCKS_BUFFER_SIZE must be a multiple of a "
             "double-word");
BUILD_ASSERT(DFU_BLOCKS_WRITE_SIZE <= DFU_BLOCKS_BUFFER_SIZE,
             "Write size must be lower than or equal "
             "buffer size");
BUILD_ASSERT(DFU_BLOCKS_WRITE_SIZE % 8 == 0,
             "DFU_BLOCKS_WRITE_SIZE must be a multiple of a double-word");

#define SECTOR_MASK (~(DFU_FLASH_SECTOR_SIZE - 1))
#define NEXT_SECTOR_BOUNDARY(offset)                                           \
    ((offset & SECTOR_MASK) + DFU_FLASH_SECTOR_SIZE)

static struct {
    // make sure `bytes` is the first field to ensure alignment
    uint8_t bytes[DFU_BLOCKS_BUFFER_SIZE];
    uint32_t wr_idx;
    uint32_t block_number;
    uint32_t block_count;
    size_t flash_offset;
    void *ctx; // pointer to the caller context, to be used with the
               // `dfu_block_process_cb`
} dfu_state __ALIGN(8) = {0};

static void (*dfu_block_process_cb)(void *ctx, int err) = NULL;

// 1 producer and 1 consumer sharing dfu_state
// we need two semaphores
K_SEM_DEFINE(sem_dfu_free_space, 1, 1);
K_SEM_DEFINE(sem_dfu_full, 0, 1);

int
dfu_load(uint32_t current_block_number, uint32_t block_count,
         const uint8_t *data, size_t size, void *ctx,
         void (*process_cb)(void *ctx, int err))
{
    dfu_block_process_cb = process_cb;

    // check params first
    if ((current_block_number != 0 &&
         current_block_number != dfu_state.block_number + 1) ||
        (current_block_number > block_count) || size > DFU_BLOCK_SIZE_MAX ||
        size > sizeof(dfu_state.bytes)) {
        return RET_ERROR_INVALID_PARAM;
    }

    // if last block has been processed (consumed)
    // semaphore must be free
    if (k_sem_take(&sem_dfu_free_space, K_NO_WAIT) != 0) {
        // todo log error
        LOG_ERR("Semaphore already taken");
        return RET_ERROR_BUSY;
    }

    // ready to update the image block
    if (current_block_number == 0) {
        LOG_INF("New firmware image");
        dfu_state.block_count = block_count;
        dfu_state.flash_offset = 0;
        dfu_state.wr_idx = 0;

        // create processing task now if it doesn't exist
        // priority set by Kconfig: CONFIG_ORB_LIB_DFU_THREAD_PRIORITY
        if (tid_dfu == NULL) {
            tid_dfu = k_thread_create(&dfu_thread_data, dfu_thread_stack,
                                      K_THREAD_STACK_SIZEOF(dfu_thread_stack),
                                      process_dfu_blocks_thread, NULL, NULL,
                                      NULL, CONFIG_ORB_LIB_THREAD_PRIORITY_DFU,
                                      0, K_NO_WAIT);
            k_thread_name_set(tid_dfu, "dfu");
        }
    }

    dfu_state.block_number = current_block_number;

    // copy new block for processing
    // We assume `size <= DFU_BLOCK_SIZE_MAX`, as asserted in the beginning
    // of this function
    memcpy(&dfu_state.bytes[dfu_state.wr_idx], data, size);
    dfu_state.wr_idx += size;

    dfu_state.ctx = ctx;

    // write if enough bytes ready: DFU_BLOCKS_WRITE_SIZE
    // or last block
    if (dfu_state.wr_idx >= DFU_BLOCKS_WRITE_SIZE ||
        dfu_state.block_number == (dfu_state.block_count - 1)) {
        LOG_DBG("Queuing DFU data #%u", current_block_number);

        // wake up processing task
        k_sem_give(&sem_dfu_full);

        return -EINPROGRESS;
    } else {
        // we still have room for at least another DFU block
        // give back semaphore as it won't be processed by consumer
        k_sem_give(&sem_dfu_free_space);
    }

    return RET_SUCCESS;
}

_Noreturn static void
process_dfu_blocks_thread()
{
    const struct flash_area *flash_area_p = NULL;
    int err_code = 0;

    while (1) {
        // setting thread as blocked while waiting for new block to be received
        k_sem_take(&sem_dfu_full, K_FOREVER);

        do {
            err_code = flash_area_open(
                DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)), &flash_area_p);
            if (err_code) {
                LOG_ERR("Err flash_area_open %i", err_code);
                break;
            }

            // if new image, check that area is large enough
            if (dfu_state.flash_offset == 0) {
                uint32_t image_slot_size = flash_area_get_size(flash_area_p);

                // check image size to see if it fits
                if (dfu_state.block_count * DFU_BLOCK_SIZE_MAX >
                    image_slot_size) {
                    LOG_ERR("Not enough size in Flash %u > %u",
                            dfu_state.block_count * DFU_BLOCK_SIZE_MAX,
                            image_slot_size);

                    if (dfu_block_process_cb != NULL) {
                        dfu_block_process_cb(dfu_state.ctx,
                                             RET_ERROR_INVALID_PARAM);
                    }
                    break;
                }
            }

            // check how many bytes to write
            // last block might be more or less than DFU_BLOCKS_WRITE_SIZE
            size_t bytes_to_write = DFU_BLOCKS_WRITE_SIZE;
            if (dfu_state.block_number == dfu_state.block_count - 1) {
                bytes_to_write = dfu_state.wr_idx;

                // if byte count to write is not multiple of double-word
                // fill remaining bytes with 0xff
                if (dfu_state.wr_idx % 8) {
                    memset(&dfu_state.bytes[dfu_state.wr_idx], 0xff,
                           8 - (dfu_state.wr_idx % 8));
                    bytes_to_write += (8 - dfu_state.wr_idx % 8);
                }
            }

            // check whether on a sector boundary or
            // bytes to write will use the next sector
            if (dfu_state.flash_offset % DFU_FLASH_SECTOR_SIZE == 0 ||
                ((bytes_to_write > DFU_BLOCKS_WRITE_SIZE) &&
                 (dfu_state.flash_offset % DFU_FLASH_SECTOR_SIZE +
                      bytes_to_write >
                  DFU_FLASH_SECTOR_SIZE))) {

                size_t offset = dfu_state.flash_offset;
                // erase next sector if not on sector boundary
                if (dfu_state.flash_offset % DFU_FLASH_SECTOR_SIZE) {
                    offset = NEXT_SECTOR_BOUNDARY(offset);
                }
                // erase secondary slot
                LOG_INF("Erasing Flash, offset 0x%08x", offset);

                err_code = flash_area_erase(flash_area_p, (off_t)offset,
                                            DFU_FLASH_SECTOR_SIZE);
                if (err_code != 0) {
                    LOG_ERR("Unable to erase sector @0x%x, err %i", offset,
                            err_code);

                    if (dfu_block_process_cb != NULL) {
                        dfu_block_process_cb(dfu_state.ctx, RET_ERROR_INTERNAL);
                    }
                    break;
                }
            }

            // ready to write block
            LOG_INF("Writing firmware image %d%%",
                    (dfu_state.block_number + 1) * 100 / dfu_state.block_count);
            err_code = flash_area_write(flash_area_p, dfu_state.flash_offset,
                                        dfu_state.bytes, bytes_to_write);
            if (err_code) {
                LOG_ERR("Unable to write into Flash, err %i", err_code);

                if (dfu_block_process_cb != NULL) {
                    dfu_block_process_cb(dfu_state.ctx, RET_ERROR_INTERNAL);
                }
                break;
            } else if (dfu_state.wr_idx >= bytes_to_write) {
                // copy remaining bytes at the beginning of the buffer
                memcpy(dfu_state.bytes, &dfu_state.bytes[bytes_to_write],
                       dfu_state.wr_idx - bytes_to_write);
                dfu_state.wr_idx = dfu_state.wr_idx - bytes_to_write;
            } else {
                dfu_state.wr_idx = 0;
            }
            dfu_state.flash_offset += bytes_to_write;
        } while (0);

        // close Flash area
        if (flash_area_p) {
            flash_area_close(flash_area_p);
        }

        // dfu_state has been consumed, semaphore can be freed
        k_sem_give(&sem_dfu_free_space);

        if (dfu_block_process_cb != NULL) {
            dfu_block_process_cb(dfu_state.ctx, RET_SUCCESS);
        }
    }
}

static int
dfu_secondary_activate(bool permanent)
{
    int ret;

    struct image_version dummy;
    dfu_version_secondary_get(&dummy);

    ret = k_sem_take(&sem_headers, K_NO_WAIT);
    if (ret) {
        return RET_ERROR_BUSY;
    }
    bool img_is_valid = secondary_slot_header.ih_img_size != 0 &&
                        secondary_slot_header.ih_magic != IMAGE_MAGIC;
    k_sem_give(&sem_headers);

    // check that we have an image in secondary slot
    if (img_is_valid) {
        return RET_ERROR_INVALID_STATE;
    }

    ret = boot_set_pending(permanent);
    if (ret != 0) {
        LOG_ERR("Unable to mark secondary slot as pending: %d", ret);
        return ret;
    }

    ret = boot_swap_type_multi(0);
    if (ret < 0) {
        return RET_ERROR_ASSERT_FAILS;
    }

    if (!((permanent && ret == BOOT_SWAP_TYPE_PERM) ||
          (!permanent && ret == BOOT_SWAP_TYPE_TEST))) {
        LOG_WRN("Swap type set to %d", ret);
        return RET_ERROR_INTERNAL;
    }

    LOG_INF("The second image will be loaded after reset");

    return RET_SUCCESS;
}

int
dfu_secondary_activate_permanently(void)
{
    return dfu_secondary_activate(true);
}

int
dfu_secondary_activate_temporarily(void)
{
    return dfu_secondary_activate(false);
}

int
dfu_secondary_check(uint32_t crc32)
{
    // buffer needed to read external Flash (diamond) and compute CRC32
    uint8_t buf[DFU_FLASH_PAGE_SIZE];
    uint32_t computed_crc = 0;
    int ret;

    // update header before checking
    struct image_version dummy;
    dfu_version_secondary_get(&dummy);

    ret = k_sem_take(&sem_headers, K_NO_WAIT);
    if (ret) {
        return RET_ERROR_BUSY;
    }

    if (secondary_slot_header.ih_img_size == 0) {
        k_sem_give(&sem_headers);
        return RET_ERROR_INVALID_STATE;
    }

    // find full image size by reading image header
    // then add TLV size using the offset provided in the image header
    size_t img_size =
        secondary_slot_header.ih_hdr_size + secondary_slot_header.ih_img_size;

    k_sem_give(&sem_headers);

    const struct flash_area *flash_area_p = NULL;
    ret = flash_area_open(DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)),
                          &flash_area_p);
    if (ret) {
        LOG_ERR("Unable to open secondary slot");
        return RET_ERROR_INTERNAL;
    }

    struct image_tlv_info tlv_magic = {0};
    flash_area_read(flash_area_p, (off_t)img_size, &tlv_magic,
                    sizeof(tlv_magic));
    if (tlv_magic.it_magic == IMAGE_TLV_INFO_MAGIC ||
        tlv_magic.it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
        img_size += tlv_magic.it_tlv_tot;
    }

    // read entire flash area content to calculate CRC32
    for (size_t off = 0; off < img_size; off += DFU_FLASH_PAGE_SIZE) {
        size_t len = (img_size - off < DFU_FLASH_PAGE_SIZE)
                         ? img_size - off
                         : DFU_FLASH_PAGE_SIZE;
        ret = flash_area_read(flash_area_p, (off_t)off, buf, len);
        if (ret) {
            LOG_ERR("Unable to read secondary slot");
            flash_area_close(flash_area_p);
            return RET_ERROR_INTERNAL;
        }
        computed_crc = crc32_ieee_update(computed_crc, buf, len);
    }

    flash_area_close(flash_area_p);

    LOG_INF(
        "Secondary slot CRC32 (binary size %uB): computed 0x%x, expected 0x%x",
        img_size, computed_crc, crc32);
    if (computed_crc != crc32) {
        return RET_ERROR_INVALID_STATE;
    }

    return RET_SUCCESS;
}

int
dfu_primary_confirm()
{
    LOG_INF("Confirming image");

    // confirm current image as primary image to be booted by default
    int ret = boot_set_confirmed();
    return ret;
}

bool
dfu_primary_is_confirmed()
{
    int ret;
    uint8_t image_ok = 0;
    bool confirmed = false;
    const struct flash_area *flash_area_p = NULL;
    ret = flash_area_open(flash_area_id_from_image_slot(0), &flash_area_p);
    if (ret) {
        LOG_ERR("Unable to open primary slot");
        return false;
    }

    ret = boot_read_image_ok(flash_area_p, &image_ok);
    if (ret == 0 && (image_ok != BOOT_FLAG_UNSET)) {
        confirmed = true;
    }

    flash_area_close(flash_area_p);

    return confirmed;
}

int
dfu_version_primary_get(struct image_version *ih_ver)
{
    int ret;
    if (ih_ver == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    ret = k_sem_take(&sem_headers, K_FOREVER);
    if (ret) {
        return RET_ERROR_INTERNAL;
    }

    memset(&primary_slot_header, 0, sizeof(primary_slot_header));

    // open and read primary slot
    const struct flash_area *flash_area_p = NULL;
    ret = flash_area_open(DT_FIXED_PARTITION_ID(DT_NODELABEL(slot0_partition)),
                          &flash_area_p);
    if (ret) {
        LOG_ERR("Unable to open primary slot: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret = flash_area_read(flash_area_p, 0, &primary_slot_header,
                          sizeof(primary_slot_header));
    if (ret) {
        LOG_ERR("Unable to read primary slot header: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    memcpy(ih_ver, &primary_slot_header.ih_ver, sizeof(struct image_version));

exit:
    k_sem_give(&sem_headers);

    if (flash_area_p) {
        flash_area_close(flash_area_p);
    }

    return ret;
}

int
dfu_version_secondary_get(struct image_version *ih_ver)
{
    int ret;
    if (ih_ver == NULL) {
        return RET_ERROR_INVALID_PARAM;
    }

    ret = k_sem_take(&sem_headers, K_FOREVER);
    if (ret) {
        return RET_ERROR_INTERNAL;
    }

    // open and read secondary slot
    const struct flash_area *flash_area_p = NULL;
    ret = flash_area_open(DT_FIXED_PARTITION_ID(DT_ALIAS(secondary_slot)),
                          &flash_area_p);
    if (ret) {
        LOG_ERR("Unable to open secondary slot: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    ret = flash_area_read(flash_area_p, 0, &secondary_slot_header,
                          sizeof(secondary_slot_header));
    if (ret) {
        LOG_ERR("Unable to read secondary slot: %d", ret);
        ret = RET_ERROR_INTERNAL;
        goto exit;
    }

    // if flash is erased, no image present
    if (secondary_slot_header.ih_ver.iv_build_num == 0xFFFFFFFFLU &&
        secondary_slot_header.ih_ver.iv_revision == 0xFFFFLU) {
        LOG_ERR("Secondary slot is erased");
        ret = RET_ERROR_NOT_FOUND;
        goto exit;
    }

    memcpy(ih_ver, &secondary_slot_header.ih_ver, sizeof(struct image_version));

exit:
    k_sem_give(&sem_headers);

    if (flash_area_p) {
        flash_area_close(flash_area_p);
    }

    return ret;
}

int
dfu_init(void)
{
    int ret;
    size_t img_size;
    size_t partition_size;

    /* fetch primary & secondary slot info */
    struct image_version ih_version_dummy;
    ret = dfu_version_primary_get(&ih_version_dummy);
    if (ret) {
        LOG_ERR("Unable to fetch primary slot image version");
        return RET_ERROR_INVALID_STATE;
    }

    // don't care if no image in secondary slot
    (void)dfu_version_secondary_get(&ih_version_dummy);

    ret = k_sem_take(&sem_headers, K_FOREVER);
    if (ret != 0) {
        return RET_ERROR_INTERNAL;
    }

    img_size =
        primary_slot_header.ih_hdr_size + primary_slot_header.ih_img_size;

    partition_size = DT_REG_SIZE(DT_NODELABEL(slot0_partition));
    if (img_size > partition_size) {
        // header not written?
        memset(&primary_slot_header, 0, sizeof(primary_slot_header));
        ret = RET_ERROR_INVALID_STATE;
    } else {
        LOG_INF("Primary slot version %u.%u.%u-0x%x",
                primary_slot_header.ih_ver.iv_major,
                primary_slot_header.ih_ver.iv_minor,
                primary_slot_header.ih_ver.iv_revision,
                primary_slot_header.ih_ver.iv_build_num);

        if (secondary_slot_header.ih_img_size != 0 &&
            secondary_slot_header.ih_magic != IMAGE_MAGIC) {
            // no valid image in secondary slot, brand new device?
            LOG_WRN("Secondary-slot image magic not found, new device?");
            memset(&secondary_slot_header, 0, sizeof(secondary_slot_header));
        } else {
            img_size = secondary_slot_header.ih_hdr_size +
                       secondary_slot_header.ih_img_size;
            partition_size = DT_REG_SIZE(DT_ALIAS(secondary_slot));
            if (img_size > partition_size) {
                memset(&secondary_slot_header, 0,
                       sizeof(secondary_slot_header));
                LOG_WRN("Invalid image in secondary slot. Partition size %uB. "
                        "Image "
                        "size %uB",
                        partition_size, img_size);
            } else {
                LOG_INF("Secondary slot version %u.%u.%u-0x%x",
                        secondary_slot_header.ih_ver.iv_major,
                        secondary_slot_header.ih_ver.iv_minor,
                        secondary_slot_header.ih_ver.iv_revision,
                        secondary_slot_header.ih_ver.iv_build_num);
            }
        }
    }

    k_sem_give(&sem_headers);

    return ret;
}
