#pragma once

#include <mcu.pb.h>

bool
pong_received(void);

void
ping_pong_reset();

int
ping_received(orb_mcu_Ping *ping);

int
ping_pong_send_mcu(orb_mcu_Ping *ping);

void
ping_init(int (*send_fn)(orb_mcu_Ping *ping));
