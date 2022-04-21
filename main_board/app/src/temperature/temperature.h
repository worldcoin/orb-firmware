#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdint.h>

void
temperature_set_sampling_period_ms(uint32_t sample_period);

void
temperature_start(void);

void
temperature_init(void);

#endif // TEMPERATURE_H
