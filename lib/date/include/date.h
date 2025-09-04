#pragma once

#include <stdint.h>
#include <time.h>

/**
 * Get date
 * @return timestamp (epoch), or 0 if date hasn't been set
 */
uint64_t
date_get(void);

void
date_print(void);

int
date_set_time_epoch(const time_t epoch);

int
date_set_time(struct tm *tm_time);
