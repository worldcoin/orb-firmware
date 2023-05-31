#ifndef IR_CAMERA_SYSTEM_INTERNAL_USE
#error This file should not be included outside of the IR Camera System!
#endif

#pragma once

#include <stdbool.h>

bool
get_focus_sweep_in_progress(void);
void
set_focus_sweep_in_progress(void);
void
clear_focus_sweep_in_progress(void);

bool
get_mirror_sweep_in_progress(void);
void
set_mirror_sweep_in_progress(void);
void
clear_mirror_sweep_in_progress(void);

void
ir_camera_system_enable_ir_eye_camera_force(void);
void
ir_camera_system_disable_ir_eye_camera_force(void);
