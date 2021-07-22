//
// Created by Cyril on 21/07/2021.
//

#ifndef ORB_FIRMWARE_ORB_COMMON_HARDFAULT_H
#define ORB_FIRMWARE_ORB_COMMON_HARDFAULT_H

#include <stdint.h>

typedef struct __attribute__((packed)) context_state_frame {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t return_address;
    uint32_t xpsr;
} context_state_frame_t;

#define HARDFAULT_HANDLING_ASM(_x)               \
  __asm volatile(                                \
      "tst lr, #4 \n"                            \
      "ite eq \n"                                \
      "mrseq r0, msp \n"                         \
      "mrsne r0, psp \n"                         \
      "b hardfault_handler_c \n"                 \
                                                 )

void
hardfault_handler_c(context_state_frame_t *frame);

#endif //ORB_FIRMWARE_ORB_COMMON_HARDFAULT_H
