//
// Created by Cyril on 09/09/2021.
//

#ifndef ORB_FIRMWARE_ORB_COMMON_BOARDS_H
#define ORB_FIRMWARE_ORB_COMMON_BOARDS_H

#ifdef PROTO_V2
#include "board_proto2.h"
#elif defined(STM32G4_DISCOVERY)
#include "board_stm32g4discovery.h"
#else
#warning "Please specify a board"
#endif

#endif //ORB_FIRMWARE_ORB_COMMON_BOARDS_H
