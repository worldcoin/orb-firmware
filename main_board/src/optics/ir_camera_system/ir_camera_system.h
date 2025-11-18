#pragma once

#include <errors.h>
#include <mcu.pb.h>
#include <zephyr/sys/util.h>

#define MAX_NUMBER_OF_FOCUS_VALUES                                             \
    (STRUCT_MEMBER_SIZE_BYTES(orb_mcu_main_IREyeCameraFocusSweepLensValues,    \
                              focus_values) /                                  \
     (sizeof(uint16_t)))

#ifdef CONFIG_ZTEST
void
ir_camera_test_reset(void *fixture);

#ifndef CONFIG_SHELL
// in twister tests we don't want to wait for too long
#define IR_LED_AUTO_OFF_TIMEOUT_S (5) // ztest - 5 seconds
#endif
#endif

#ifndef IR_LED_AUTO_OFF_TIMEOUT_S
// automatically turn off IR LEDs after 60 seconds without any activity
#define IR_LED_AUTO_OFF_TIMEOUT_S (60)
#endif

/**
 * @brief Enable the IR eye camera
 *
 * @retval RET_ERROR_BUSY Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS new settings applied
 * @retval RET_ERROR_NOT_INITIALIZED The IR camera system is not initialized,
 * use @c ir_camera_system_init() first
 */
ret_code_t
ir_camera_system_enable_ir_eye_camera(void);

/**
 * @brief Disable the IR eye camera
 *
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_disable_ir_eye_camera(void);

/**
 * @brief Check if the IR eye camera is enabled
 *
 * @return true if the IR eye camera is enabled, false otherwise
 */
bool
ir_camera_system_ir_eye_camera_is_enabled(void);

/**
 * @brief Enable the IR face camera
 *
 * @retval RET_ERROR_BUSY Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS new settings applied
 * @retval RET_ERROR_NOT_INITIALIZED The IR camera system is not initialized,
 * use @c ir_camera_system_init() first
 */
ret_code_t
ir_camera_system_enable_ir_face_camera(void);

/** Same as ir_camera_system_enable_ir_face_camera()
 */
ret_code_t
ir_camera_system_enable_rgb_face_camera(void);
ret_code_t
ir_camera_system_disable_rgb_face_camera(void);

/**
 * @brief Disable the IR face camera
 *
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_disable_ir_face_camera(void);

/**
 * @brief Check if the IR face camera is enabled
 *
 * @return true if the IR face camera is enabled, false otherwise
 */
bool
ir_camera_system_ir_face_camera_is_enabled(void);

/**
 * @brief Enable the 2D time-of-flight camera
 *
 * @retval RET_ERROR_BUSY Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS new settings applied
 * @retval RET_ERROR_NOT_INITIALIZED The IR camera system is not initialized,
 * use @c ir_camera_system_init() first
 */
ret_code_t
ir_camera_system_enable_2d_tof_camera(void);

/**
 * @brief Disable the 2D time-of-flight camera
 *
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress and the enabled camera may not be changed until the operation is
 * completed
 * @retval RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_disable_2d_tof_camera(void);

/**
 * @brief Check if the 2D time-of-flight camera is enabled
 *
 * @return true if the 2D time-of-flight camera is enabled, false otherwise
 */
bool
ir_camera_system_2d_tof_camera_is_enabled(void);

/**
 * @brief Enable a group of IR LEDs, or disable them.
 *
 * Passing WAVELENGTH_NONE will disable LEDs and is accepted in any condition.
 *
 * @retval RET_SUCCESS LEDs enabled at corresponding @c wavelength
 * @retval RET_ERROR_NOT_INITIALIZED not initialized
 * @retval RET_ERROR_BUSY focus sweep or mirror sweep in progress
 */
ret_code_t
ir_camera_system_enable_leds(orb_mcu_main_InfraredLEDs_Wavelength wavelength);

/**
 * @brief Get enabled LED wavelengths
 * @return orb_mcu_main_InfraredLEDs_Wavelength
 */
orb_mcu_main_InfraredLEDs_Wavelength
ir_camera_system_get_enabled_leds(void);

/**
 * Returns the time in microseconds until the next update of the
 * CAMERA_TRIGGER_TIMER happens.
 * This timer triggers all cameras and the IR LEDs at the update event.
 * @return time until update in us
 */
uint32_t
ir_camera_system_get_time_until_update_us(void);

/**
 * Set cameras' Frames-Per-Second value
 * Settings are computed if:
 * - IR-LEDs duty cycle (on-time) <= 15% (Pearl) OR <= 25% (Diamond)
 * AND
 * - the maximum Frames-Per-Second of 60Hz is not exceeded.
 * It is allowed to reset the FPS to 0 in any condition.
 * @param fps Frames-Per-Second, maximum is 60
 * @retval RET_ERROR_INVALID_PARAM: FPS value isn't valid: max FPS or max duty
 *         cycle exceeded
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 *         progress
 * @retval RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_set_fps(uint16_t fps);

/**
 * Get cameras' Frames-Per-Second value
 * @return Frames-Per-Second, maximum is 60
 */
uint16_t
ir_camera_system_get_fps(void);

/**
 * Set IR LEDs on duration for 940nm and 850nm LEDs
 * Settings are computed if a duty cycle <= 15% (Pearl) / 25% (Diamond) and a
 * maximum on-time of 5000 us (pearl) / 8000 us (diamond) is not exceeded.
 * Exceeding the maximum on-time will result in unchanged settings and an
 * RET_ERROR_INVALID_PARAM.
 * It is allowed to reset the duration to 0 in any condition.
 *
 * @param on_time_us LED on duration, maximum is 5000 (pearl) / 8000 (diamond)
 * @retval RET_ERROR_INVALID_PARAM: \c on_time_us isn't valid, max duty cycle or
 * max on-time exceeded
 * @retval RET_SUCCESS: new settings applied
 */
ret_code_t
ir_camera_system_set_on_time_us(uint16_t on_time_us);

/**
 * Set the focus values (target current in mA) for the liquid lens to be used
 * during a focus sweep operation.
 * The maximum number of values may not exceed MAX_NUMBER_OF_FOCUS_VALUES.
 * @param focus_values the array of focus values
 * @param num_focus_values the length of the focus_values array
 * @retval RET_SUCCESS: focus values saved
 * @retval RET_ERROR_INVALID_PARAM: num_focus_values >
 * MAX_NUMBER_OF_FOCUS_VALUES
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress
 */
ret_code_t
ir_camera_system_set_focus_values_for_focus_sweep(int16_t *focus_values,
                                                  size_t num_focus_values);

/**
 * Set the focus values (target current in mA) for the liquid lens to be used
 * during a focus sweep operation
 * @param poly the polynomial coefficients
 * @retval RET_SUCCESS: focus values saved
 * @retval RET_ERROR_BUSY: Some other operation (like a focus sweep) is in
 * progress
 */
ret_code_t
ir_camera_system_set_polynomial_coefficients_for_focus_sweep(
    orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial poly);

/**
 * Perform a focus sweep using the IR eye camera. Using this function has
 * several requirements.
 * 1. A sweep must not be already in progress.
 * 2. FPS must be set to something greater than zero.
 * 3. The IR eye camera must be disabled.
 *
 * While the sweep is running, several things are not allowed:
 * 1. The FPS may not be changed.
 * 2. The liquid lens focus value may not be changed.
 * 3. Wavelengths may not be enabled/disabled.
 * 4. The IR eye camera may not be enabled/disabled.
 *
 * This function is asynchronous and returns immediately.
 *
 * @retval RET_SUCCESS: All is good, focus sweep is initiated.
 * @retval RET_ERROR_BUSY: A sweep is already in progress.
 * @retval RET_ERROR_INVALID_STATE: Either the FPS is zero or the IR eye camera
 * is enabled.
 */
ret_code_t
ir_camera_system_perform_focus_sweep(void);

/**
 * Set the mirror position values for use
 * during a mirror sweep operation
 * @param poly the polynomial coefficients
 * @retval RET_SUCCESS: focus values saved
 * @retval RET_ERROR_BUSY: Some other operation (like a mirror sweep) is in
progress
 */
ret_code_t
ir_camera_system_set_polynomial_coefficients_for_mirror_sweep(
    orb_mcu_main_IREyeCameraMirrorSweepValuesPolynomial poly);

/**
 * Perform a mirror sweep using the IR eye camera. Using this function has
 * several requirements.
 * 1. A sweep must not be already in progress.
 * 2. FPS must be set to something greater than zero.
 * 3. The IR eye camera must be disabled.
 *
 * While the sweep is running, several things are not allowed:
 * 1. The FPS may not be changed.
 * 2. The mirror angles may not be changed.
 * 3. Wavelengths may not be enabled/disabled.
 * 4. The IR eye camera may not be enabled/disabled.
 *
 * This function is asynchronous and returns immediately.
 *
 * @retval RET_SUCCESS: All is good, focus sweep is initiated.
 * @retval RET_ERROR_BUSY: A sweep is already in progress.
 * @retval RET_ERROR_INVALID_STATE: Either the FPS is zero or the IR eye camera
 * is enabled.
 */
ret_code_t
ir_camera_system_perform_mirror_sweep(void);

/**
 * Determine the state/status of the IR camera system.
 *
 * @retval RET_SUCCESS: IR camera system is initialized and is not performing an
 *  uninterruptible function.
 * @retval RET_ERROR_NOT_INITIALIZED: The IR camera system is not initialized
 * and one needs to call `ir_camera_system_init()`.
 * @retval RET_ERROR_BUSY: Some uninterruptible function is in progress, like a
 * focus sweep. Said function will terminate eventually.
 * @retval RET_ERROR_FORBIDDEN: distance measured in front by 1d-tof is too
 * close, or PVCC is not enabled because safety circuitry tripped.
 */
ret_code_t
ir_camera_system_get_status(void);

/**
 * @brief Initialize the IR camera system
 * @retval RET_SUCCESS The IR camera system is initialized
 * @retval RET_ERROR_ALREADY_INITIALIZED The IR camera system is already
 * initialized
 * @retval RET_ERROR_INTERNAL The IR camera system failed to initialize: pins or
 * timers cannot be configured, see ir_camera_system_hw_init(void)
 */
ret_code_t
ir_camera_system_init(void);
