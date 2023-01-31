#ifndef IR_CAMERA_SYSTEM
#define IR_CAMERA_SYSTEM

#include <errors.h>
#include <mcu_messaging.pb.h>
#include <zephyr/sys/util.h>

#define MAX_NUMBER_OF_FOCUS_VALUES                                             \
    (STRUCT_MEMBER_SIZE_BYTES(IREyeCameraFocusSweepLensValues, focus_values) / \
     (sizeof(uint16_t)))

ret_code_t
ir_camera_system_init(void);

/**
 * @return error code:
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 * and the enabled camera may not be changed until the operation is completed
 *   - RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_enable_ir_eye_camera(void);
/**
 * @return error code:
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 * and the enabled camera may not be changed until the operation is completed
 *   - RET_SUCCESS: new settings applied
 */
ret_code_t
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

/**
 * @param wavelength The wavelength to enable
 * @return error code:
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 * and the enabled wavelength may not be changed until the operation is
 * completed
 *   - RET_SUCCESS: new settings applied
 */
ret_code_t
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
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
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

/**
 * Set the focus values (target current in mA) for the liquid lens to be used
 * during a focus sweep operation.
 * The maximum number of values may not exceed MAX_NUMBER_OF_FOCUS_VALUES.
 * @param focus_values the array of focus values
 * @param num_focus_values the length of the focus_values array
 * @return error code:
 *   - RET_SUCCESS: focus values saved
 *   - RET_ERROR_INVALID_PARAM: num_focus_values > MAX_NUMBER_OF_FOCUS_VALUES
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 */
ret_code_t
ir_camera_system_set_focus_values_for_focus_sweep(int16_t *focus_values,
                                                  size_t num_focus_values);

/**
 * Set the focus values (target current in mA) for the liquid lens to be used
 * during a focus sweep operation
 * @param poly the polynomial coefficients
 * @return error code:
 *   - RET_SUCCESS: focus values saved
 *   - RET_ERROR_BUSY: Some other operation (like a focus sweep) is in progress
 */
ret_code_t
ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
    IREyeCameraFocusSweepValuesPolynomial poly);

/**
 * Perform a focus sweep using the IR eye camera. Using this function has
 * several requirements.
 * 1. A focus sweep must not be already in progress.
 * 2. FPS must be set to something greater than zero.
 * 3. The IR eye camera must be disabled.
 *
 * While the sweep is running, several things are not allowed:
 * 1. The FPS may not be changed.
 * 2. The liquid lens focus value may not be changed.
 * 3. Wavelegnths may not be enabled/disabled.
 * 4. The IR eye camera may not be enabled/disabled.
 *
 * This function is asynchronous and returns immediately.
 *
 * @return error code:
 *  - RET_SUCCESS: All is good, focus sweep is initiated.
 *  - RET_ERROR_BUSY: A focus sweep is already in progress.
 *  - RET_ERROR_INVALID_STATE: Either the FPS is zero or the IR eye camera is
 * enabled.
 */
ret_code_t
ir_camera_system_perform_focus_sweep(void);

/**
 * @return true is a focus sweep is in progress.
 */
bool
ir_camera_system_is_focus_sweep_in_progress(void);

#endif // IR_CAMERA_SYSTEM
