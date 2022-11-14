#ifndef BU27030_H
#define BU27030_H

#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/util.h>

#define BU27030_SYSTEM_CONTROL (0x40)
#define BU27030_PART_ID        (0x12)

#define BU27030_MODE_CONTROL2 (0x42)
#define BU27030_MODE_CONTROL3 (0x43)

#define BU27030_DATA0_LOW_BYTE  (0x50)
#define BU27030_DATA0_HIGH_BYTE (0x51)

#define BU27030_DATA1_LOW_BYTE  (0x52)
#define BU27030_DATA1_HIGH_BYTE (0x53)

struct bu27030_config {
    struct i2c_dt_spec i2c;
};

struct bu27030_data {
    uint16_t data0;
    uint16_t data1;
    uint8_t gain; // Can be 1, 32, 256
};

#endif // BU27030_H
