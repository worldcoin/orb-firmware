#include "dfu.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "compilers.h"
#include "errno.h"
#include "messaging/incoming_message_handling.h"
#include "power_sequence/power_sequence.h"
#include <app_config.h>
#include <devicetree.h>
#include <flash_map_backend/flash_map_backend.h>
#include <logging/log.h>
#include <messaging/messaging.h>
#include <storage/flash_map.h>
#include <sys/crc.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(dfu);

static void
process_dfu_blocks_thread();

K_THREAD_STACK_DEFINE(dfu_thread_stack, THREAD_STACK_SIZE_DFU_PROCESS_BLOCK);
static struct k_thread dfu_thread_data = {0};
static k_tid_t tid_dfu = NULL;

static struct image_header *primary_slot;
static struct image_header *secondary_slot;

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

static_assert(DFU_BLOCKS_BUFFER_SIZE % 8 == 0,
              "DFU_BLOCKS_BUFFER_SIZE must be a multiple of a "
              "double-word");
static_assert(DFU_BLOCKS_WRITE_SIZE <= DFU_BLOCKS_BUFFER_SIZE,
              "Write size must be lower than or equal "
              "buffer size");
static_assert(DFU_BLOCKS_WRITE_SIZE % 8 == 0,
              "DFU_BLOCKS_WRITE_SIZE must be a multiple of a double-word");

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 4096
#endif

#define PAGE_MASK         (~(FLASH_PAGE_SIZE - 1))
#define NEXT_PAGE(offset) ((offset & PAGE_MASK) + FLASH_PAGE_SIZE)

static struct {
    // make sure `bytes` is the first field to ensure alignment
    uint8_t bytes[DFU_BLOCKS_BUFFER_SIZE];
    uint32_t wr_idx;
    uint32_t last_ack_number;
    uint32_t block_number;
    uint32_t block_count;
    uint32_t flash_offset;
} dfu_state __ALIGN(8) = {0};

// 1 producer and 1 consumer sharing dfu_state
// we need two semaphores
K_SEM_DEFINE(sem_dfu_free_space, 1, 1);
K_SEM_DEFINE(sem_dfu_full, 0, 1);

/**
 * Queue new Firmware image block for processing
 * @param msg
 */
int
dfu_load(uint32_t current_block_number, uint32_t block_count,
         const uint8_t *data, size_t size, uint32_t ack_number)
{
    // check params first
    if ((current_block_number != 0 &&
         current_block_number != dfu_state.block_number + 1) ||
        (current_block_number > block_count) ||
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

        // create higher-priority processing task now if it doesn't exist
        if (tid_dfu == NULL) {
            tid_dfu = k_thread_create(&dfu_thread_data, dfu_thread_stack,
                                      K_THREAD_STACK_SIZEOF(dfu_thread_stack),
                                      process_dfu_blocks_thread, NULL, NULL,
                                      NULL, THREAD_PRIORITY_DFU_PROCESS_BLOCK,
                                      0, K_NO_WAIT);
            if (!tid_dfu) {
                LOG_ERR("ERROR spawning dfu thread");
            }
        }
    }

    dfu_state.block_number = current_block_number;

    // copy new block for processing
    // We assume `size <= DFU_BLOCK_SIZE_MAX`, as asserted in the beginning
    // of this function
    memcpy(&dfu_state.bytes[dfu_state.wr_idx], data, size);
    dfu_state.wr_idx += size;

    dfu_state.last_ack_number = ack_number;

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
            err_code = flash_area_open(flash_area_id_from_image_slot(1),
                                       &flash_area_p);
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
                    LOG_ERR("Not enough size in Flash %lu > %u",
                            dfu_state.block_count * DFU_BLOCK_SIZE_MAX,
                            image_slot_size);

                    incoming_message_ack(Ack_ErrorCode_RANGE,
                                         dfu_state.last_ack_number);
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
            if (dfu_state.flash_offset % FLASH_PAGE_SIZE == 0 ||
                ((bytes_to_write > DFU_BLOCKS_WRITE_SIZE) &&
                 (dfu_state.flash_offset % FLASH_PAGE_SIZE + bytes_to_write >
                  FLASH_PAGE_SIZE))) {

                uint32_t offset = dfu_state.flash_offset;
                // erase next sector if not on sector boundary
                if (dfu_state.flash_offset % FLASH_PAGE_SIZE) {
                    offset = NEXT_PAGE(offset);
                }
                // erase secondary slot
                LOG_INF("Erasing Flash, offset 0x%08x", offset);

                err_code =
                    flash_area_erase(flash_area_p, offset, FLASH_PAGE_SIZE);
                if (err_code != 0) {
                    LOG_ERR("Unable to erase sector, err %i", err_code);

                    incoming_message_ack(Ack_ErrorCode_RANGE,
                                         dfu_state.last_ack_number);
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

                incoming_message_ack(Ack_ErrorCode_FAIL,
                                     dfu_state.last_ack_number);
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

        incoming_message_ack(Ack_ErrorCode_SUCCESS, dfu_state.last_ack_number);
    }
}

static int
dfu_secondary_activate(bool permanent)
{
    // check that we have an image in secondary slot
    if (secondary_slot->ih_magic != IMAGE_MAGIC) {
        return RET_ERROR_INVALID_STATE;
    }

    int ret = boot_set_pending(permanent);
    if (ret == 0) {
        LOG_INF("The second image will be loaded after reset");

        // wait for Jetson to shut down before we can reboot
        power_reboot_set_pending();
    } else {
        LOG_ERR("Unable to mark secondary slot as pending");
    }

    return ret;
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
    if (secondary_slot == NULL) {
        return RET_ERROR_INVALID_STATE;
    }

    // find full image size by reading image header
    // then add TLV size using the offset provided in the image header
    size_t img_size = secondary_slot->ih_hdr_size + secondary_slot->ih_img_size;

    struct image_tlv_info *tlv_magic =
        (struct image_tlv_info *)&((uint8_t *)secondary_slot)[img_size];
    if (tlv_magic->it_magic == IMAGE_TLV_INFO_MAGIC ||
        tlv_magic->it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
        img_size += tlv_magic->it_tlv_tot;
    }

    uint32_t computed = crc32_ieee((uint8_t *)secondary_slot, img_size);

    LOG_INF(
        "Secondary slot CRC32 (binary size %uB): computed 0x%x, expected 0x%x",
        img_size, computed, crc32);
    if (computed != crc32) {
        return RET_ERROR_INVALID_STATE;
    }
    return RET_SUCCESS;
}

int
dfu_primary_confirm()
{
    // confirm current image as primary image to be booted by default
    int ret = boot_set_confirmed();
    return ret;
}

int
dfu_versions_send()
{
    McuMessage versions = {
        .which_message = McuMessage_m_message_tag,
        .message.m_message.which_payload = McuToJetson_versions_tag,
        .message.m_message.payload.versions.has_primary_app = true,
        .message.m_message.payload.versions.primary_app.major =
            primary_slot->ih_ver.iv_major,
        .message.m_message.payload.versions.primary_app.minor =
            primary_slot->ih_ver.iv_minor,
        .message.m_message.payload.versions.primary_app.patch =
            primary_slot->ih_ver.iv_revision,
        .message.m_message.payload.versions.has_secondary_app = true,
        .message.m_message.payload.versions.secondary_app.major =
            secondary_slot->ih_ver.iv_major,
        .message.m_message.payload.versions.secondary_app.minor =
            secondary_slot->ih_ver.iv_minor,
        .message.m_message.payload.versions.secondary_app.patch =
            secondary_slot->ih_ver.iv_revision,
#if defined(CONFIG_BOARD_MCU_MAIN_V30)
        .message.m_message.payload.versions.hardware_version = 30,
#elif defined(CONFIG_BOARD_MCU_MAIN_V31)
        .message.m_message.payload.versions.hardware_version = 31,
#endif
    };
    return messaging_push_tx(&versions);
}

int
dfu_init(void)
{
    uintptr_t flash_base_addr = 0;

    int ret = flash_device_base(0, &flash_base_addr);
    __ASSERT(ret == 0, "Failed loading Flash base address");

    primary_slot =
        (struct image_header *)(flash_base_addr +
                                DT_REG_ADDR(DT_NODELABEL(slot0_partition)));
    secondary_slot =
        (struct image_header *)(flash_base_addr +
                                DT_REG_ADDR(DT_NODELABEL(slot1_partition)));

    LOG_INF("Primary slot version %u.%u.%u-%u", primary_slot->ih_ver.iv_major,
            primary_slot->ih_ver.iv_minor, primary_slot->ih_ver.iv_revision,
            primary_slot->ih_ver.iv_build_num);

    LOG_INF("Secondary slot version %u.%u.%u-%u",
            secondary_slot->ih_ver.iv_major, secondary_slot->ih_ver.iv_minor,
            secondary_slot->ih_ver.iv_revision,
            secondary_slot->ih_ver.iv_build_num);

    return 0;
}
