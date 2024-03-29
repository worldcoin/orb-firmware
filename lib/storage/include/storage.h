#pragma once

#include "compilers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <zephyr/devicetree.h>

/**
 * First In First Out storage
 * Dynamically sized records can be written
 * Read records are the oldest and can be marked as "processed" or "read" by
 * freeing them (ie invalidating).
 *
 * STM32 flash only allows for writing an entire double word with 0 in case
 * dword isn't erased so an invalid record has its full header set to zeros.
 */

#define FLASH_WRITE_BLOCK_SIZE                                                 \
    DT_PROP_OR(DT_PARENT(DT_PARENT(DT_NODELABEL(storage_partition))),          \
               write_block_size, 1)

enum magic_state_e {
    RECORD_UNUSED = 0xFFFF,  //!< Unused bytes in Flash (erased)
    RECORD_VALID = 0xFEFE,   //!< Valid record to be used
    RECORD_INVALID = 0x0000, //!< Freed record
};

/**
 * Header that allows for verifications over the records
 * such as state (valid or invalid) & CRC check
 * Even though the structure is simple, we prefer to pack the data
 * to make sure it takes the expected number of bytes
 */
typedef struct __PACKED __may_alias {
    uint16_t magic_state;
    uint16_t record_size;
    uint16_t crc16;  //!< CRC over the data, including padding which can be used
                     //!< to align the record on Flash write size
    uint16_t unused; //!< 0xffff
} storage_header_t;

/// Pointers to read/write through the flash area
struct storage_area_s {
    const struct flash_area *fa;
    off_t wr_idx; //!< offset into the flash area
    off_t rd_idx; //!< offset into the flash area
};

/**
 * Write new record to storage
 *
 * @note Writing into Flash is done per-block meaning the returned record
 *       might be larger than the stored record.
 *
 * @param record Pointer to record to be stored in Flash
 * @param size Size of the record
 * @retval RET_SUCCESS record stored
 * @retval RET_ERROR_NO_MEM flash area doesn't have enough empty space to store
 *  the new record
 * @retval RET_ERROR_INTERNAL error writing flash
 * @retval RET_ERROR_INVALID_STATE CRC16 computed over flash content doesn't
 * match CRC16 computed over \c record content
 * @retval RET_ERROR_NOT_INITIALIZED storage area not initialized
 */
int
storage_push(char *record, size_t size);

/**
 * Peek oldest record without invalidating it.
 * Record is copied into buffer given its maximum size passed as a parameter.
 * @param buffer Buffer to hold the oldest stored record
 * @param size To be set to \c buffer size and is modified to the read record
 *             size for the caller to correctly use the buffer
 * @retval RET_SUCCESS buffers contains the oldest stored record with the given
 *    size
 * @retval RET_ERROR_NOT_FOUND storage is empty
 * @retval RET_ERROR_NO_MEM \c buffer cannot hold the record with given \c
 *    size
 * @retval RET_ERROR_INVALID_STATE CRC failure, use \c storage_free to discard
 * the data
 * @retval RET_ERROR_NOT_INITIALIZED storage area not initialized
 */
int
storage_peek(char *buffer, size_t *size);

/**
 * Invalidate oldest record. The record will then be considered stale.
 *
 * @note Data header is zeroed but the record isn't modified to reduce flash
 *       wear
 *
 * @retval RET_SUCCESS record invalidated and read index pushed to next record
 * @retval RET_ERROR_NOT_FOUND record pointed by read index isn't valid
 * @retval RET_ERROR_INTERNAL error invalidating record in Flash
 * @retval RET_ERROR_NOT_INITIALIZED storage area not initialized
 */
int
storage_free(void);

/**
 * Check if storage has content to be used
 * @retval true if storage contains data
 * @retval false otherwise
 */
bool
storage_has_data(void);

/**
 * Initialize storage area by looking for contiguous valid records in the Flash
 * area
 * @retval RET_SUCCESS Storage correctly initialized and ready to use
 * @retval RET_ERROR_NOT_INITIALIZED Unable to open flash area
 */
int
storage_init(void);

#if CONFIG_ORB_LIB_STORAGE_TESTS
/// Tests
void
get_storage_area(struct storage_area_s *area);
#endif
