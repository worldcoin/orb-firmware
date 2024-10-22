#pragma once

#include <stdint.h>

typedef struct {
    uint16_t focus_values[200];
} orb_mcu_main_IREyeCameraFocusSweepLensValues;

typedef struct {
} orb_mcu_main_IREyeCameraFocusSweepValuesPolynomial;

typedef struct {
} orb_mcu_main_IREyeCameraMirrorSweepValuesPolynomial;

typedef enum {
    orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_NONE,
    orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_ONE,
    orb_mcu_main_InfraredLEDs_Wavelength_WAVELENGTH_TWO,
} orb_mcu_main_InfraredLEDs_Wavelength;
