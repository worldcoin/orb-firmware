#include "nfc.h"
#include "app_assert.h"
#include "errors.h"
#include "orb_logs.h"
#include "orb_state.h"
#include "system/version/version.h"
#include "zephyr/drivers/i2c.h"
#include <stdint.h>
#include <zephyr/device.h>

ORB_STATE_REGISTER(nfc);
LOG_MODULE_REGISTER(nfc);

/*
 * FYI: ST25R3916 & ST25R3918 are pretty similar chips,
 * st25 rfal use definitions below for both chips.
 * the ST25R3918 is used on the Orbs
 */

/* Identity register */
#define ST25R3916_REG_IC_IDENTITY 0x3FU
/* Puts the chip in default state (same as after power-up) */
#define ST25R3916_CMD_SET_DEFAULT 0xC1U

#define ST25R3916_WRITE_MODE (0U << 6) /*!< ST25R3916 Operation Mode: Write */
#define ST25R3916_READ_MODE  (1U << 6) /*!< ST25R3916 Operation Mode: Read */

#define ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916 (5U << 3)
#define ST25R3916_REG_IC_IDENTITY_ic_type_mask      (0x1fU << 3)

const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(nfc));
static struct k_mutex *i2c1_mutex = NULL;

static int
st25r3918_read_register(uint8_t reg, uint8_t *value)
{
    reg |= ST25R3916_READ_MODE;
    if (i2c1_mutex) {
        k_mutex_lock(i2c1_mutex, K_FOREVER);
    }
    int ret = i2c_write_read_dt(&i2c_dev, &reg, 1, value, 1);
    if (i2c1_mutex) {
        k_mutex_unlock(i2c1_mutex);
    }
    return ret;
}

int
nfc_self_test(struct k_mutex *mutex)
{
    /* nfc (re)introduced on mainboard 4.6+ */
    const orb_mcu_Hardware hw = version_get();
    if (hw.version < orb_mcu_Hardware_OrbVersion_HW_VERSION_DIAMOND_V4_6) {
        ORB_STATE_SET_CURRENT(RET_SUCCESS, "no nfc on mainboard prior to 4.6");
        return 0;
    }

    if (mutex) {
        i2c1_mutex = mutex;
    }

    int ret;

    if (!device_is_ready(i2c_dev.bus)) {
        ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE, "I2C bus not ready");
        return RET_ERROR_NOT_INITIALIZED;
    }

    /* Put the chip into the default state via direct command before reading ID
     */
    {
        uint8_t cmd = (uint8_t)(ST25R3916_CMD_SET_DEFAULT);

        if (i2c1_mutex) {
            k_mutex_lock(i2c1_mutex, K_FOREVER);
        }
        ret = i2c_write_dt(&i2c_dev, &cmd, 1);
        if (i2c1_mutex) {
            k_mutex_unlock(i2c1_mutex);
        }
        if (ret) {
            ORB_STATE_SET_CURRENT(RET_ERROR_INVALID_STATE,
                                  "SET_DEFAULT cmd failed (%d)", ret);
            return ret;
        }
    }

    uint8_t id = 0;
    ret = st25r3918_read_register(ST25R3916_REG_IC_IDENTITY, &id);
    if (ret == 0 && (id & ST25R3916_REG_IC_IDENTITY_ic_type_mask) ==
                        ST25R3916_REG_IC_IDENTITY_ic_type_st25r3916) {
        ORB_STATE_SET_CURRENT(RET_SUCCESS, "identity ok: 0x%02x", id);
    } else {
        ORB_STATE_SET_CURRENT(RET_ERROR_ASSERT_FAILS,
                              "identity check failed: 0x%x (%d)", id, ret);
    }

    return ret;
}

#if CONFIG_ZTEST
#include <zephyr/ztest.h>

ZTEST(hardware, test_nfc)
{
    int ret = nfc_self_test(NULL);
    zassert_true(ret == 0, "nfc self test failed");
}
#endif
