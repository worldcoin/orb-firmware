#ifndef IR_CAMERA_SYSTEM
#define IR_CAMERA_SYSTEM

#include <errors.h>
#include <mcu_messaging.pb.h>
#include <sys/util.h>

#define IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US 5000

ret_code_t
ir_camera_system_init(void);

void
ir_camera_system_enable_ir_eye_camera(void);
void
ir_camera_system_disable_ir_eye_camera(void);
bool
ir_camera_system_ir_eye_camera_is_enabled(void);

void
ir_camera_system_enable_ir_face_camera(void);
void
ir_camera_system_disable_ir_face_camera(void);
bool
ir_camera_system_ir_face_camera_is_enabled(void);

void
ir_camera_system_enable_2d_tof_camera(void);
void
ir_camera_system_disable_2d_tof_camera(void);
bool
ir_camera_system_2d_tof_camera_is_enabled(void);

void
ir_camera_system_enable_leds(InfraredLEDs_Wavelength wavelength);
InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void);

// 0 - 100%
ret_code_t
ir_camera_system_set_740nm_led_brightness(uint32_t percentage);

void
ir_camera_system_set_fps(uint16_t fps);

ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us);

#endif // IR_CAMERA_SYSTEM
