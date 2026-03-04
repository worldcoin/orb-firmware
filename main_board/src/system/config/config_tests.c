#include "config.h"
#include "orb_logs.h"
#include <errors.h>
#include <storage.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(config_tests, CONFIG_CONFIG_LOG_LEVEL);

void
clean_config(void *fixture)
{
    ARG_UNUSED(fixture);

    const struct flash_area *fa;

    int ret = flash_area_open(FIXED_PARTITION_ID(config_partition), &fa);
    if (ret == 0) {
        ret = flash_area_erase(fa, 0, fa->fa_size);
        zassert_equal(ret, 0, "flash_area_erase failed %d", ret);
        flash_area_close(fa);
    }

    if (ret != 0) {
        LOG_ERR("Unable to erase config partition for unit tests");
    }

    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);
}

ZTEST(config, test_defaults_after_init)
{
    /* After a clean init on erased flash, boot behavior must be BOOT_BUTTON */
    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_BUTTON, "expected BOOT_BUTTON (0), got %d",
                  behavior);
}

ZTEST(config, test_set_get_boot_always)
{
    int ret = config_set_reboot_behavior(BOOT_ALWAYS);
    zassert_equal(ret, RET_SUCCESS, "config_set_reboot_behavior failed %d",
                  ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_ALWAYS, "expected BOOT_ALWAYS (1), got %d",
                  behavior);
}

ZTEST(config, test_set_get_boot_button)
{
    /* First set to BOOT_ALWAYS, then back to BOOT_BUTTON */
    int ret = config_set_reboot_behavior(BOOT_ALWAYS);
    zassert_equal(ret, RET_SUCCESS, "set BOOT_ALWAYS failed %d", ret);

    ret = config_set_reboot_behavior(BOOT_BUTTON);
    zassert_equal(ret, RET_SUCCESS, "set BOOT_BUTTON failed %d", ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_BUTTON, "expected BOOT_BUTTON (0), got %d",
                  behavior);
}

ZTEST(config, test_persistence_across_reinit)
{
    /* Write BOOT_ALWAYS, re-init, verify it was loaded from flash */
    int ret = config_set_reboot_behavior(BOOT_ALWAYS);
    zassert_equal(ret, RET_SUCCESS, "config_set_reboot_behavior failed %d",
                  ret);

    /* Re-initialize from flash (simulates reboot) */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_ALWAYS,
                  "expected BOOT_ALWAYS after re-init, got %d", behavior);
}

ZTEST(config, test_no_op_same_value)
{
    /* Set BOOT_ALWAYS twice — second call should be a no-op (no flash write) */
    int ret = config_set_reboot_behavior(BOOT_ALWAYS);
    zassert_equal(ret, RET_SUCCESS, "first set failed %d", ret);

    ret = config_set_reboot_behavior(BOOT_ALWAYS);
    zassert_equal(ret, RET_SUCCESS, "second set (same value) failed %d", ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_ALWAYS, "expected BOOT_ALWAYS, got %d",
                  behavior);

    /* Re-init to confirm flash still has the correct single record */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_ALWAYS,
                  "expected BOOT_ALWAYS after re-init, got %d", behavior);
}

ZTEST(config, test_drain_multiple_records)
{
    /*
     * Simulate a scenario where multiple records exist in the partition
     * (e.g. from interrupted writes).  Push several records directly via
     * the storage API, then verify config_init drains to the last one.
     */
    const struct flash_area *fa;
    int ret = flash_area_open(FIXED_PARTITION_ID(config_partition), &fa);
    zassert_equal(ret, 0, "flash_area_open failed %d", ret);
    ret = flash_area_erase(fa, 0, fa->fa_size);
    zassert_equal(ret, 0, "flash_area_erase failed %d", ret);
    flash_area_close(fa);

    /* Use a raw storage area to push multiple config records */
    struct storage_area_s raw_area;
    ret = storage_init(&raw_area, FIXED_PARTITION_ID(config_partition));
    zassert_equal(ret, RET_SUCCESS, "storage_init failed %d", ret);

    /* Record 1: BOOT_BUTTON */
    reboot_behavior_t val = BOOT_BUTTON;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 1 failed %d", ret);

    /* Record 2: BOOT_ALWAYS */
    val = BOOT_ALWAYS;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 2 failed %d", ret);

    /* Record 3: BOOT_BUTTON (this should be the one that wins) */
    val = BOOT_BUTTON;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 3 failed %d", ret);

    /* Now init config — it should drain records 1 & 2, keep record 3 */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_BUTTON,
                  "expected BOOT_BUTTON (last record), got %d", behavior);

    /* Verify only one record remains by re-initializing again */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "second config_init failed %d", ret);

    behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_BUTTON,
                  "expected BOOT_BUTTON after second re-init, got %d",
                  behavior);
}

ZTEST(config, test_multiple_transitions)
{
    /* Toggle between values several times, verify persistence each time */
    reboot_behavior_t values[] = {BOOT_ALWAYS, BOOT_BUTTON, BOOT_ALWAYS,
                                  BOOT_BUTTON, BOOT_ALWAYS};

    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        int ret = config_set_reboot_behavior(values[i]);
        zassert_equal(ret, RET_SUCCESS, "set failed at iteration %zu: %d", i,
                      ret);

        reboot_behavior_t behavior = config_get_reboot_behavior();
        zassert_equal(behavior, values[i],
                      "get mismatch at iteration %zu: expected %d, got %d", i,
                      values[i], behavior);
    }

    /* Re-init and verify last value survived */
    int ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    reboot_behavior_t behavior = config_get_reboot_behavior();
    zassert_equal(behavior, BOOT_ALWAYS,
                  "expected BOOT_ALWAYS after re-init, got %d", behavior);
}
