#include "config.h"
#include "main.pb.h"
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
    /* After a clean init on erased flash, boot behavior must be
     * orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS */
    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS (0), "
        "got %d",
        behavior);
}

ZTEST(config, test_set_get_boot_always)
{
    int ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON);
    zassert_equal(ret, RET_SUCCESS, "config_set_reboot_behavior failed %d",
                  ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON "
        "(1), got %d",
        behavior);
}

ZTEST(config, test_set_get_boot_button)
{
    /* First set to orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
     * then back to orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS */
    int ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON);
    zassert_equal(
        ret, RET_SUCCESS,
        "set orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON failed "
        "%d",
        ret);

    ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS);
    zassert_equal(
        ret, RET_SUCCESS,
        "set orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS failed %d",
        ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS (0), "
        "got %d",
        behavior);
}

ZTEST(config, test_persistence_across_reinit)
{
    /* Write orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON, re-init,
     * verify it was loaded from flash */
    int ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON);
    zassert_equal(ret, RET_SUCCESS, "config_set_reboot_behavior failed %d",
                  ret);

    /* Re-initialize from flash (simulates reboot) */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON "
        "after re-init, got %d",
        behavior);
}

ZTEST(config, test_no_op_same_value)
{
    /* Set orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON twice —
     * second call should be a no-op (no flash write) */
    int ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON);
    zassert_equal(ret, RET_SUCCESS, "first set failed %d", ret);

    ret = config_set_reboot_behavior(
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON);
    zassert_equal(ret, RET_SUCCESS, "second set (same value) failed %d", ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON, "
        "got %d",
        behavior);

    /* Re-init to confirm flash still has the correct single record */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    behavior = config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON "
        "after re-init, got %d",
        behavior);
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

    /* Record 1: orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS */
    orb_mcu_main_SetConfig_RebootBehavior val =
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 1 failed %d", ret);

    /* Record 2: orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON */
    val = orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 2 failed %d", ret);

    /* Record 3: orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS (this
     * should be the one that wins) */
    val = orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS;
    ret = storage_push(&raw_area, (char *)&val, sizeof(val));
    zassert_equal(ret, RET_SUCCESS, "push record 3 failed %d", ret);

    /* Now init config — it should drain records 1 & 2, keep record 3 */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS "
        "(last record), got %d",
        behavior);

    /* Verify only one record remains by re-initializing again */
    ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "second config_init failed %d", ret);

    behavior = config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS "
        "after second re-init, got %d",
        behavior);
}

ZTEST(config, test_multiple_transitions)
{
    /* Toggle between values several times, verify persistence each time */
    orb_mcu_main_SetConfig_RebootBehavior values[] = {
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_BUTTON_PRESS,
        orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON};

    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        int ret = config_set_reboot_behavior(values[i]);
        zassert_equal(ret, RET_SUCCESS, "set failed at iteration %zu: %d", i,
                      ret);

        orb_mcu_main_SetConfig_RebootBehavior behavior =
            config_get_reboot_behavior();
        zassert_equal(behavior, values[i],
                      "get mismatch at iteration %zu: expected %d, got %d", i,
                      values[i], behavior);
    }

    /* Re-init and verify last value survived */
    int ret = config_init();
    zassert_equal(ret, RET_SUCCESS, "config_init failed %d", ret);

    orb_mcu_main_SetConfig_RebootBehavior behavior =
        config_get_reboot_behavior();
    zassert_equal(
        behavior, orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON,
        "expected orb_mcu_main_SetConfig_RebootBehavior_BOOT_AUTO_ALWAYS_ON "
        "after re-init, got %d",
        behavior);
}
