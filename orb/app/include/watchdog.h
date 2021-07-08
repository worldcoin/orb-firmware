//
// Created by Cyril on 08/07/2021.
//

#ifndef WORLDCOIN_FIRMWARE_ORB_APP_WATCHDOG_H
#define WORLDCOIN_FIRMWARE_ORB_APP_WATCHDOG_H

#include <stdint.h>

/// IWDG Set Period in Milliseconds
/// Based on libopencm3 (https://libopencm3.org/docs/latest/stm32f3/html/iwdg__common__all_8c_source.html#l00044)
///
/// The countdown period is converted into count and prescale values. The maximum
/// period is 32.76 seconds; values above this are truncated. Periods less than 1ms
/// are not supported by this library.
///
/// A delay of up to 5 clock cycles of the LSI clock (about 156 microseconds)
/// can occasionally occur if the prescale or preload registers are currently busy
/// loading a previous value.
///
/// \param period_ms Period in milliseconds (< 32760) from a watchdog
///// reset until a system reset is issued.
void
watchdog_init(uint32_t period_ms);

/// Reload the watchdog timer, to be called regularly
void
watchdog_reload(void);

#endif //WORLDCOIN_FIRMWARE_ORB_APP_WATCHDOG_H
