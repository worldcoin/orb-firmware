#include "config.h"
#include "orb_logs.h"
#include <errors.h>
#include <storage.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(config, CONFIG_CONFIG_LOG_LEVEL);

/**
 * @brief Persistent configuration stored in the config flash partition
 *
 * This structure is stored in the `config_partition` flash area and
 * survives reboots and power cycles. It is protected by a CRC16.
 *
 * /!\ When adding new fields, append them at the end of the struct to
 * maintain backward compatibility. New fields in a larger record will
 * be zero-initialized when reading an older (smaller) config.
 */
typedef struct {
    reboot_behavior_t boot;
} persistent_config_t;

static struct storage_area_s config_storage_area;
static persistent_config_t current_config;
static bool initialized;

static K_MUTEX_DEFINE(config_mutex);

/**
 * Write the current in-memory config to flash, freeing the old record first.
 * Caller must hold config_mutex.
 *
 * @retval RET_SUCCESS on success
 * @retval other on flash error
 */
static int
persist_config(void)
{
    /*
     * Free the old record first, then push the new one.
     *
     * storage_free() may trigger an erase of the entire partition when
     * free space is below MINIMUM_EMPTY_SPACE.  If we pushed first and
     * the subsequent free erased the partition, we'd lose the just-written
     * record.  By freeing first, any erase happens on stale data, and the
     * push that follows guarantees a copy always exists on flash.
     *
     * If power is lost between free and push, config_init() will find no
     * record and fall back to defaults — acceptable since the caller's
     * in-memory state (current_config) was already updated.
     */
    if (storage_has_data(&config_storage_area)) {
        int free_ret = storage_free(&config_storage_area);
        if (free_ret != RET_SUCCESS && free_ret != RET_ERROR_NOT_FOUND) {
            LOG_WRN("failed to free old config record: %d", free_ret);
        }
    }

    /*
     * storage_push uses the record buffer internally for read-back
     * verification, so we need a mutable copy.
     */
    persistent_config_t tmp;
    memcpy(&tmp, &current_config, sizeof(tmp));

    int ret = storage_push(&config_storage_area, (char *)&tmp, sizeof(tmp));
    if (ret != RET_SUCCESS) {
        LOG_ERR("config write failed: %d", ret);
    }

    return ret;
}

int
config_init(void)
{
    k_mutex_lock(&config_mutex, K_FOREVER);

    memset(&current_config, 0, sizeof(current_config));
    initialized = false;

    int ret = storage_init(&config_storage_area,
                           FIXED_PARTITION_ID(config_partition));
    if (ret != RET_SUCCESS) {
        LOG_ERR("failed to init config storage: %d", ret);
        goto out;
    }

    /*
     * The storage is a FIFO — there may be more than one record if a
     * previous config_set was interrupted or if an older firmware left
     * stale entries.  Free all records except the last one, then peek
     * the surviving record to load the config.  This avoids an
     * unnecessary erase + re-write cycle.
     */
    while (storage_has_data(&config_storage_area) &&
           !storage_is_last_record(&config_storage_area)) {
        ret = storage_free(&config_storage_area);
        if (ret != RET_SUCCESS) {
            LOG_WRN("config drain: free error %d", ret);
            break;
        }
    }

    /* Read the single remaining record (if any) */
    persistent_config_t tmp = {0};
    size_t size = sizeof(tmp);
    ret = storage_peek(&config_storage_area, (char *)&tmp, &size);
    if (ret == RET_SUCCESS) {
        memcpy(&current_config, &tmp, sizeof(current_config));
        LOG_INF("config loaded (boot=%d)", current_config.boot);
    } else if (ret == RET_ERROR_NOT_FOUND) {
        LOG_INF("no valid config found, using defaults");
    } else {
        LOG_WRN("config read error %d, using defaults", ret);
    }

    initialized = true;
    ret = RET_SUCCESS;

out:
    k_mutex_unlock(&config_mutex);
    return ret;
}

reboot_behavior_t
config_get_reboot_behavior(void)
{
    __ASSERT(initialized, "config module not initialized");
    return current_config.boot;
}

int
config_set_reboot_behavior(reboot_behavior_t behavior)
{
    k_mutex_lock(&config_mutex, K_FOREVER);

    if (!initialized) {
        k_mutex_unlock(&config_mutex);
        return RET_ERROR_NOT_INITIALIZED;
    }

    if (current_config.boot == behavior) {
        k_mutex_unlock(&config_mutex);
        return RET_SUCCESS;
    }

    current_config.boot = behavior;

    int ret = persist_config();
    if (ret == RET_SUCCESS) {
        LOG_INF("config updated (boot=%d)", current_config.boot);
    }

    k_mutex_unlock(&config_mutex);
    return ret;
}
