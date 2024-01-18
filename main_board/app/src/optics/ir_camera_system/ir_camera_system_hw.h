#ifndef IR_CAMERA_SYSTEM_INTERNAL_USE
#error This file should not be included outside of the IR Camera System!
#endif

#pragma once

ret_code_t
ir_camera_system_hw_init(void);

/* Eye camera */
void
ir_camera_system_enable_ir_eye_camera_hw(void);
void
ir_camera_system_disable_ir_eye_camera_hw(void);

/* Face camera */
void
ir_camera_system_enable_ir_face_camera_hw(void);
void
ir_camera_system_disable_ir_face_camera_hw(void);

/* 2d tof camera */
void
ir_camera_system_enable_2d_tof_camera_hw(void);
void
ir_camera_system_disable_2d_tof_camera_hw(void);

/* Other */

void
ir_camera_system_enable_leds_hw(void);
ret_code_t
ir_camera_system_set_fps_hw(uint16_t fps);
ret_code_t
ir_camera_system_set_on_time_us_hw(uint16_t on_time_us);
ret_code_t
ir_camera_system_set_on_time_740nm_us_hw(uint16_t on_time_us);

#if defined(ZTEST)
// mock the function to avoid including the ir_camera_system_hw module
// in tests
inline uint32_t
ir_camera_system_get_time_until_update_us_internal(void)
{
    return 0;
}
#else
uint32_t
ir_camera_system_get_time_until_update_us_internal(void);
#endif

/* Focus sweep */
void
ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw(
    IREyeCameraFocusSweepValuesPolynomial poly);
void
ir_camera_system_set_focus_values_for_focus_sweep_hw(int16_t *focus_values,
                                                     size_t num_focus_values);
void
ir_camera_system_perform_focus_sweep_hw(void);

/* Mirror sweep */
void
ir_camera_system_set_polynomial_coefficients_for_mirror_sweep_hw(
    IREyeCameraMirrorSweepValuesPolynomial poly);
void
ir_camera_system_perform_mirror_sweep_hw(void);

uint16_t
ir_camera_system_get_fps_hw(void);
