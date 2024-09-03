#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Platform overrides for the default configuration settings in the
//! memfault-firmware-sdk. Default configuration settings can be found in
//! "memfault/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_MEMFAULT)

// Heartbeat interval is 15 minutes
// These will be called:
//  memfault_metrics_heartbeat_collect_sdk_data();
//  memfault_metrics_heartbeat_collect_data();
#define MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS (60 * 15)

#endif // CONFIG_MEMFAULT

#ifdef __cplusplus
}
#endif
