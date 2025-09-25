#pragma once

#include <zephyr/kernel.h>

/**
 * Scan device on i2c bus to check if it responds
 *
 * @return 0 on success, error code on error
 */
int
nfc_self_test(struct k_mutex *mutex);
