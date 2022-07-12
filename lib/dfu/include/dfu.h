#ifndef ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H
#define ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H

#include "bootutil/image.h"
#include "utils.h"
#include <mcu_messaging.pb.h>
#include <stdbool.h>

#define DFU_BLOCK_SIZE_MAX                                                     \
    STRUCT_MEMBER_ARRAY_SIZE(FirmwareUpdateData, image_block.bytes)

/// Queue one firmware image block.
/// An internal buffer is used to queue blocks before writing to flash a larger,
/// memory-aligned chunk.
/// \param current_block_number Must increment for each new
/// block. 0 will erase the flash area to receive a new image
/// \param block_count
/// Number of image blocks to be processed
/// \param data Firmware image block
/// \param size Size of the image block
/// \param ctx Pointer to the caller context, if needed when calling \c
/// process_cb
/// \param process_cb Callback when block is processed asynchronously, with
/// error code
/// \return
/// * RET_ERROR_INVALID_PARAM \c current_block_number or size invalid
/// * RET_ERROR_BUSY last block not processed
/// * -EINPROGRESS block queued for further processing, \c process_cb will be
/// called to provide the actual status
/// * RET_SUCCESS block processed
int
dfu_load(uint32_t current_block_number, uint32_t block_count,
         const uint8_t *data, size_t size, void *ctx,
         void (*process_cb)(void *ctx, int err));

/// Activate image in secondary slot.
/// The image will be used after system reset.
/// The image won't be marked for testing but used permanently after first boot
/// \return
int
dfu_secondary_activate_permanently(void);

/// Activate image in secondary slot.
/// The image will _confirm_ itself, meaning it will be run for testing first
/// and will set itself to be used permanently, ideally after some internal
/// see \c dfu_primary_confirm
/// tests
/// \return
int
dfu_secondary_activate_temporarily(void);

/// Check image in secondary slot against a CRC32.
/// Use it to validate a new image has correctly been written on Flash.
/// \param crc32
/// \return
/// * RET_ERROR_INVALID_STATE secondary slot not initialized or CRC32 does not
/// match
/// * RET_SUCCESS CRC32 match successfully
int
dfu_secondary_check(uint32_t crc32);

/// Confirm image in primary slot: will set the image as working accross reboot
/// when image is being tested.
/// \return 0 on success; nonzero on failure. See \c boot_set_confirmed_multi
int
dfu_primary_confirm(void);

/// Check the status of the running image.
/// \return
/// * \c true: Confirmed = permanently used.
/// * \c false: Testing = will be reverted after system reset if not confirmed
bool
dfu_primary_is_confirmed();

/// Get image version in primary slot (running image)
int
dfu_version_primary_get(struct image_version *ih_ver);

/// Get image version in secondary slot
int
dfu_version_secondary_get(struct image_version *ih_ver);

/// Init module
int
dfu_init(void);

#endif // ORB_FIRMWARE_APP_MAIN_SRC_DFU_DFU_H
