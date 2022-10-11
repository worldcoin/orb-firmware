#ifndef ORB_LIB_STORAGE_H
#define ORB_LIB_STORAGE_H

#include <stddef.h>
#include <stdint.h>

/// First In First Out storage
/// Dynamically sized records can be written
/// Read records are the oldest and can be marked as "processed" or "read" by
/// freeing them (ie invalidating). STM32 flash only allows for writing an
/// entire double word with 0 in case dword isn't erased so an invalid record
/// has its header set to zeros.

#define FLASH_WRITE_BLOCK_SIZE                                                 \
    DT_PROP(DT_CHOSEN(zephyr_flash), write_block_size)

enum magic_state_e {
    RECORD_UNUSED = 0xFFFF,  //!< Unused bytes in Flash (erased)
    RECORD_VALID = 0xFEFE,   //!< Valid record to be used
    RECORD_INVALID = 0x0000, //!< Freed record
};

/// Header that allows for verifications over the records
/// such as state (valid or invalid) & CRC check
typedef struct {
    uint16_t magic_state;
    uint16_t record_size;
    uint16_t crc16;
    uint16_t unused; // 0xffff
} storage_header_t;

/// Pointers to read/write through the flash area
struct storage_area_s {
    const struct flash_area *fa;
    storage_header_t *wr_idx; //!< write pointer, direct access to Flash content
    storage_header_t *rd_idx; //!< read pointer, direct access to Flash content
};

/// Write new record to storage
///
/// \note Size must be a multiple of \c FLASH_WRITE_BLOCK_SIZE
///       Fill remaining bytes with \c 0xff
///
/// \param record Pointer to record to be stored in Flash, padded with 0xff
/// \param size Size of the record
/// \return Error codes
///    * RET_SUCCESS record stored
///    * RET_ERROR_INVALID_PARAM if size not multiple of FLASH_WRITE_BLOCK_SIZE
///    * RET_ERROR_NO_MEM flash area doesn't have enough empty space to store
///      the new record
///    * RET_ERROR_INTERNAL error writing flash
///    * RET_ERROR_INVALID_STATE CRC16 computed over flash content doesn't match
///      CRC16 computed over \c record content
int
storage_push(char *record, size_t size);

/// Peek oldest record without invalidating it
/// \param record
/// \param size
/// \return
int
storage_peek(char *record, size_t *size);

/// Invalidate oldest record
/// Data header is zeroed but the record isn't modified to reduce flash wear
/// \return
///     * RET_ERROR_NOT_FOUND record pointed by read index isn't valid
///     * RET_ERROR_INTERNAL error invalidating record in Flash
int
storage_free(void);

/// Initialize storage area by looking for valid records in the Flash area
/// \return
int
storage_init(void);

#if CONFIG_ORB_LIB_STORAGE_TESTS
/// Tests
void
get_storage_area(struct storage_area_s *area);
#endif

#endif // ORB_LIB_STORAGE_H
