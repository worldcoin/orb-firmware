#ifndef STEPPER_MOTORS_H
#define STEPPER_MOTORS_H

#include <errors.h>

ret_code_t motors_angle_horizontal(int8_t x);
ret_code_t motors_angle_vertical(int8_t x);

ret_code_t motors_init(void);

#endif // STEPPER_MOTORS_H
