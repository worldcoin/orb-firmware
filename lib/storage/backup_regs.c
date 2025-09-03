#include "storage.h"

#include "orb_logs.h"
#include "zephyr/device.h"
#include "zephyr/drivers/bbram.h"

static const struct device *const backup_regs_dev =
    DEVICE_DT_GET(DT_NODELABEL(bbram));
LOG_MODULE_DECLARE(storage, CONFIG_STORAGE_LOG_LEVEL);

int
backup_regs_read_byte(const size_t offset, uint8_t *data)
{
    size_t size;
    int ret = bbram_get_size(backup_regs_dev, &size);
    if (ret == 0 && offset >= size) {
        return -EINVAL;
    }

    ret = bbram_read(backup_regs_dev, offset, sizeof(data), data);
    return ret;
}

int
backup_regs_write_byte(const size_t offset, const uint8_t data)
{
    size_t size;
    int ret = bbram_get_size(backup_regs_dev, &size);
    if (ret == 0 && offset >= size) {
        return -EINVAL;
    }

    ret = bbram_write(backup_regs_dev, offset, sizeof(data), &data);
    return ret;
}
