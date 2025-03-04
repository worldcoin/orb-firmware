#include "system/version/version.h"
#include "voltage_measurement.h"
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(voltage_measurement_test, LOG_LEVEL_DBG);

ZTEST(hil, test_voltage_measurements)
{
    Z_TEST_SKIP_IFNDEF(CONFIG_TEST_VOLTAGE_MEASUREMENT);

    ret_code_t ret;
    int32_t voltage_mv;

    ret = voltage_measurement_get(CHANNEL_VBAT_SW, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("VBAT_SW = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 12000, 17000);

    ret = voltage_measurement_get(CHANNEL_PVCC, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("PVCC = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 30590, 32430);

#ifdef CONFIG_BOARD_PEARL_MAIN
    ret = voltage_measurement_get(CHANNEL_12V, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("12V = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 11700, 12840);
#endif

    ret = voltage_measurement_get(CHANNEL_12V_CAPS, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("12V_CAPS = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 11700, 12280);

    ret = voltage_measurement_get(CHANNEL_3V3_UC, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("3V3_UC = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 3159, 3389);

    ret = voltage_measurement_get(CHANNEL_1V8, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("1V8 = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 1710, 1890);

    ret = voltage_measurement_get(CHANNEL_3V3, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("3V3 = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 3265, 3456);

    ret = voltage_measurement_get(CHANNEL_5V, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("5V = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 5061, 5233);

    ret = voltage_measurement_get(CHANNEL_3V3_SSD_3V8, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("3V3_SSD_3V8 = %d mV", voltage_mv);
    orb_mcu_Hardware version = version_get();
    if (version.version >= orb_mcu_Hardware_OrbVersion_HW_VERSION_PEARL_EV5) {
        zassert_between_inclusive(voltage_mv, (3300 * 0.95), (3300 * 1.05));
    } else {
        zassert_between_inclusive(voltage_mv, (3800 * 0.95), (3800 * 1.05));
    }

    ret = voltage_measurement_get(CHANNEL_VREFINT, &voltage_mv);
    zassert_equal(ret, RET_SUCCESS);
    LOG_INF("VREFINT = %d mV", voltage_mv);
    zassert_between_inclusive(voltage_mv, 1182, 1232);
}
