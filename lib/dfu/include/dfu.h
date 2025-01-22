#pragma once

#include "bootutil/image.h"
#include "utils.h"
#include <mcu.pb.h>
#include <stdbool.h>

#define DFU_BLOCK_SIZE_MAX                                                     \
    STRUCT_MEMBER_ARRAY_SIZE(orb_mcu_FirmwareUpdateData, image_block.bytes)

#if DT_NODE_HAS_COMPAT(DT_GPARENT(DT_ALIAS(secondary_slot)), jedec_spi_nor)
#include "flash/spi_nor.h"
#define DFU_FLASH_SECTOR_SIZE SPI_NOR_SECTOR_SIZE
#define DFU_FLASH_PAGE_SIZE   SPI_NOR_PAGE_SIZE
#else
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 4096
#endif
#define DFU_FLASH_SECTOR_SIZE FLASH_PAGE_SIZE
#define DFU_FLASH_PAGE_SIZE   FLASH_PAGE_SIZE
#endif

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
     written on Flash using DFU_BLOCKS_WRITE_SIZE-long                         \
     blocks. */

// make sure the DFU blocks buffer has a size multiple of double-word
// otherwise we won't be able to write the full buffer on Flash
#define DFU_BLOCKS_BUFFER_SIZE                                                 \
    (DFU_BLOCKS_BUFFER_MIN_SIZE + 8 - (DFU_BLOCKS_BUFFER_MIN_SIZE % 8))

struct dfu_state_t {
    // make sure `bytes` is the first field to ensure alignment
    uint8_t bytes[DFU_BLOCKS_BUFFER_SIZE];
    uint32_t wr_idx;
    uint32_t block_number;
    uint32_t block_count;
    size_t flash_offset;
    void *ctx; // pointer to the caller context, to be used with the
    // `dfu_block_process_cb`
};

/**
 * Queue one firmware image block.
 * An internal buffer is used to queue blocks before writing to flash a larger,
 * memory-aligned chunk.
 * @param current_block_number Must increment for each new
 * block. 0 will erase the flash area to receive a new image
 * @param block_count
 * Number of image blocks to be processed
 * @param data Firmware image block
 * @param size Size of the image block
 * @param ctx Pointer to the caller context, if needed when calling @c
 * process_cb
 * @param process_cb Callback when block is processed asynchronously, with
 * error code
 * @retval RET_ERROR_INVALID_PARAM @c current_block_number or size invalid
 * @retval RET_ERROR_BUSY last block not processed
 * @retval -EINPROGRESS block queued for further processing, @c process_cb will
 * be called to provide the actual status
 * @retval RET_SUCCESS block processed
 */
int
dfu_load(uint32_t current_block_number, uint32_t block_count,
         const uint8_t *data, size_t size, void *ctx,
         void (*process_cb)(void *ctx, int err));

/**
 * Activate image in secondary slot.
 * The image will be used after system reset.
 * The image won't be marked for testing but used permanently after first boot
 * @retval RET_SUCCESS image activated and will be installed after reboot
 * @retval RET_ERROR_INVALID_STATE secondary slot does not seem to contain a
 * valid image
 * @retval RET_ERROR_ASSERT_FAILS unable to read back the swap type
 * @retval RET_ERROR_INTERNAL swap type doesn't match the expected value
 */
int
dfu_secondary_activate_permanently(void);

/**
 * Activate image in secondary slot.
 * The image should _confirm_ itself, meaning it will be run for testing first
 * and will set itself to be used permanently, ideally after some internal tests
 * see @c dfu_primary_confirm
 * @retval RET_SUCCESS image activated and will need to confirm itself after
 * reboot to be used permanently
 * @retval RET_ERROR_INVALID_STATE secondary slot does not seem to contain a
 * valid image
 * @retval RET_ERROR_ASSERT_FAILS unable to read back the swap type
 * @retval RET_ERROR_INTERNAL swap type doesn't match the expected value
 */
int
dfu_secondary_activate_temporarily(void);

/**
 * Check image in secondary slot against a CRC32.
 * Use it to validate a new image has correctly been written on Flash.
 *
 * @warning this function allocates a flash-page sized buffer on the stack,
 *          which may be large
 *
 * @param crc32
 * @retval RET_ERROR_INVALID_STATE secondary slot not initialized or CRC32 does
 * not match
 * @retval RET_SUCCESS CRC32 match successfully
 */
int
dfu_secondary_check(uint32_t crc32);

/**
 * Confirm image in primary slot: will set the image as working accross reboot
 * when image is being tested.
 * @return 0 on success; nonzero on failure. See @c boot_set_confirmed_multi
 */
int
dfu_primary_confirm(void);

/**
 * Check the status of the running image.
 * @retval true: Confirmed = permanently used.
 * @retval false: Testing = will be reverted after system reset if not confirmed
 */
bool
dfu_primary_is_confirmed();

/**
 * Get image version in primary slot
 * @param ih_ver Pointer to the version structure to be filled
 * @retval RET_ERROR_INVALID_STATE when pointer to image header is invalid,
 * either @c dfu_init has not been called or the image size is larger than the
 * flash area (partition), see @fn dfu_init
 * @retval RET_SUCCESS on success, @c ih_ver is filled with the version
 */
int
dfu_version_primary_get(struct image_version *ih_ver);

/**
 * Get image version in secondary slot
 * @param ih_ver Pointer to the version structure to be filled
 * @retval RET_ERROR_NOT_FOUND if no image present in secondary slot
 * @retval RET_SUCCESS on success, @c ih_ver is filled with the version
 */
int
dfu_version_secondary_get(struct image_version *ih_ver);

/**
 * @brief Init module
 *
 * Initialize Flash areas by loading pointers to the primary and secondary
 * slots in order to read the associated image versions.
 * The primary slot contains the image currently running, the secondary slot
 * eventually contains an image to be used after a reset, if activated.
 */
int
dfu_init(void);
