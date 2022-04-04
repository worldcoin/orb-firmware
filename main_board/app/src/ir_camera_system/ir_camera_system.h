#ifndef IR_CAMERA_SYSTEM
#define IR_CAMERA_SYSTEM

#include <errors.h>
#include <mcu_messaging.pb.h>
#include <sys/util.h>

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

/**
 * Set cameras' Frames-Per-Second value
 * Settings are computed to turn on IR LEDs with a duty cycle of 10% and maximum
 * on-time of 5ms
 * If LED are turned off, a duty cycle of 10% will be applied or max 5ms
 * LED on-time is modified accordingly to keep a duty cycle <= 10%, max 5ms.
 * @param fps Frames-Per-Second, maximum is 60
 * @return error code:
 *   - RET_ERROR_INVALID_PARAM: FPS value isn't valid
 *   - RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_set_fps(uint16_t fps);

/**
 * Set IR LEDs on duration for 940nm and 850nm LEDs
 * Settings are computed to keep duty cycle <= 10%, or maximum on-time of 5ms
 * If LEDs are turned off, a duty cycle of 10% will be applied to set the FPS.
 * The FPS is modified accordingly to keep a duty cycle <= 10% in case
 * \c on_time_us is too large to keep the duty cycle <= 10%
 * @param on_time_us LED on duration, maximum is 5000
 * @return error code:
 *    - RET_ERROR_INVALID_PARAM: \c on_time_us isn't valid (> 5000?)
 *    - RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us);

/**
 * Set IR LEDs on duration for the 740nm LEDs
 * Settings are computed to keep duty cycle <= 45%, with no maximum on-time
 * (effectively 450ms at an FPS of 1).
 * The on time will be clamped at 45% duty cycle if on_time_us is too large.
 * @param on_time_us LED on duration
 * @return error code:
 *    - RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_set_on_time_740nm_us(uint16_t on_time_us);

#endif // IR_CAMERA_SYSTEM
