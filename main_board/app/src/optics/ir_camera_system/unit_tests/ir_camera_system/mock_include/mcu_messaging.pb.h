#pragma once

#include <stdint.h>

typedef struct {
    uint16_t focus_values[200];
} IREyeCameraFocusSweepLensValues;

typedef struct {
} IREyeCameraFocusSweepValuesPolynomial;

typedef enum {
    InfraredLEDs_Wavelength_WAVELENGTH_NONE,
    InfraredLEDs_Wavelength_WAVELENGTH_ONE,
    InfraredLEDs_Wavelength_WAVELENGTH_TWO,
} InfraredLEDs_Wavelength;
