//
// Copyright (c) 2022 Tools for Humanity. All rights reserved.
//

#ifndef ORB_MCU_SECURITY_APP_HEARTBEAT_H
#define ORB_MCU_SECURITY_APP_HEARTBEAT_H

#include <stdint.h>

/// \brief Make the heart beat with maximum delay until beat maker death
/// \param delay_s Maximum delay after which the beat maker is considered
/// unresponsive. Passing 0 stops heartbeat monitoring, or can act as a dummy,
/// acknowledged message
/// \return error code
int
heartbeat_boom(uint32_t delay_s);

/// \brief Register callback for heartbeat timeout
/// Default timeout handler is a failing assert.
/// \param timeout_cb Custom timeout handler. Passing NULL resets to default
/// handler.
void
heartbeat_register_cb(void (*timeout_cb)(void));

#endif // ORB_MCU_SECURITY_APP_HEARTBEAT_H
