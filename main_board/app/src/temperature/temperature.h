#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <mcu_messaging.pb.h>
#include <stdbool.h>
#include <stdint.h>

void
temperature_set_sampling_period_ms(uint32_t sample_period);

void
temperature_start_sending(void);

void
temperature_init(void);

void
temperature_report(Temperature_TemperatureSource source,
                   int32_t temperature_in_c);

bool
temperature_is_in_overtemp(void);

#endif // TEMPERATURE_H
