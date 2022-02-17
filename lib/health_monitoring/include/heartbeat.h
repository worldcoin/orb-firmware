//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#ifndef ORB_MCU_SECURITY_APP_HEARTBEAT_H
#define ORB_MCU_SECURITY_APP_HEARTBEAT_H

#include <stdint.h>

/// Make the heart beat with maximum delay until beat maker death
/// \param delay_s Maximum delay after which the beat maker is considered
/// unresponsive \return error code
int
heartbeat_boom(uint32_t delay_s);

/// Register callback for heartbeat timeout
/// Default timeout handler is a failing assert
/// \param timeout_cb Custom timeout handler
void
heartbeat_register_cb(void (*timeout_cb)(void));

#endif // ORB_MCU_SECURITY_APP_HEARTBEAT_H
