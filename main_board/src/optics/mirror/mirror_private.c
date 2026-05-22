#include "mirror_private.h"

#include <math.h>
#include <utils.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

const int32_t mirror_center_angles[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES,
    [MOTOR_PHI_ANGLE] = MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES};

const double microsteps_per_mm = 12800.0; // 1mm / 0.4mm (pitch) * (360° / 18°
// (per step)) * 256 micro-steps

static const struct device *stepper_devs[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = DEVICE_DT_GET(DT_NODELABEL(motor_theta)),
    [MOTOR_PHI_ANGLE] = DEVICE_DT_GET(DT_NODELABEL(motor_phi)),
};

const struct device *
mirror_get_stepper_dev(motor_t motor)
{
    if (motor >= MOTORS_COUNT) {
        return NULL;
    }
    return stepper_devs[motor];
}

int32_t
calculate_millidegrees_from_center_position(
    int32_t microsteps_from_center_position, const double motors_arm_length_mm)
{
    double stepper_position_from_center_millimeters =
        (double)microsteps_from_center_position / microsteps_per_mm;
    int32_t angle_from_center_millidegrees =
        asin(stepper_position_from_center_millimeters / motors_arm_length_mm) *
        180000.0 / M_PI;
    return angle_from_center_millidegrees;
}

int32_t
calculate_microsteps_from_center_position(
    int32_t angle_from_center_millidegrees, const double motors_arm_length_mm)
{
    double stepper_position_from_center_millimeters =
        sin((double)angle_from_center_millidegrees * M_PI / 180000.0) *
        motors_arm_length_mm;
    int32_t stepper_position_from_center_microsteps = (int32_t)roundl(
        stepper_position_from_center_millimeters * microsteps_per_mm);
    return stepper_position_from_center_microsteps;
}
