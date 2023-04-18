#ifndef IR_CAMERA_SYSTEM_INTERNAL_USE
#error This file should not be included outside of the IR Camera System!
#endif

#ifndef IR_CAMERA_SYSTEM_HW
#define IR_CAMERA_SYSTEM_HW

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
ir_camera_system_enable_leds_hw(InfraredLEDs_Wavelength wavelength);
ret_code_t
ir_camera_system_set_fps_hw(uint16_t fps);
ret_code_t
ir_camera_system_set_on_time_us_hw(uint16_t on_time_us);
ret_code_t
ir_camera_system_set_on_time_740nm_us_hw(uint16_t on_time_us);

void
ir_camera_system_set_polynomial_coefficients_for_focus_sweep_hw(
    IREyeCameraFocusSweepValuesPolynomial poly);

void
ir_camera_system_set_focus_values_for_focus_sweep_hw(int16_t *focus_values,
                                                     size_t num_focus_values);

void
ir_camera_system_perform_focus_sweep_hw(void);

uint16_t
ir_camera_system_get_fps_hw(void);

#endif // IR_CAMERA_SYSTEM_HW