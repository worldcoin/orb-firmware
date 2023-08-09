#pragma once

#include <mcu_messaging.pb.h>
#include <stdbool.h>
#include <stdint.h>

void
temperature_set_sampling_period_ms(uint32_t sample_period);

void
temperature_init(const Hardware *hw_version);

void
temperature_report(Temperature_TemperatureSource source,
                   int32_t temperature_in_c);

bool
temperature_is_in_overtemp(void);
