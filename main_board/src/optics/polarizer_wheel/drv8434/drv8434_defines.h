/******************************************************************************
 * @file drv8434_defines.h
 * @brief Defines for Texas Instruments DRV8434 stepper motor driver
 *
 * This file contains various macro definitions for the DRV8434
 * stepper motor driver IC used to define bit positions and chosen configuration
 * values for registers
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLVSEG0B - September 2019 - Revised March 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

/****************************************APPLICATION DEFINES
 * *********************************************************/
// Number of DRV8434 devices this driver can support at maximum
// Daisy Chain Support NOT implemented
#define DRV8434_MAX_SUPPORTED_DEVICES 1u

// Number of bytes per SPI frame
#define DRV8434_FRAME_SIZE_BYTES 2u

// Buffer is sized for the maximum number of
// back to back frames sent
#define DRV8434_SPI_BUFFER_SIZE_MAX 2u

/****************************************SPI FRAME
 * DEFINES************************************************************/

// Start bit position of register address in SPI TX word
#define DRV8434_SPI_TX_ADDRESS_START_POS 9u

// Start bit position of R/W bit
#define DRV8434_SPI_TX_RW_START_POS 14u

// Value of R/W bit for a READ operation
#define DRV8434_SPI_TX_RW_BIT_READ 1u

// Mask for all bytes but LSB
#define DRV8434_SPI_TX_LSB_MASK 0xFFu

enum DRV8434_TRQ_DAC_Val {
    DRV8434_TRQ_DAC_100,   // 0
    DRV8434_TRQ_DAC_93P75, // 1
    DRV8434_TRQ_DAC_87P5,  // 2
    DRV8434_TRQ_DAC_81P25, // 3
    DRV8434_TRQ_DAC_75,    // 4
    DRV8434_TRQ_DAC_68P75, // 5
    DRV8434_TRQ_DAC_62P5,  // 6
    DRV8434_TRQ_DAC_56P25, // 7
    DRV8434_TRQ_DAC_50,    // 8
    DRV8434_TRQ_DAC_43P75, // 9
    DRV8434_TRQ_DAC_37P5,  // 10
    DRV8434_TRQ_DAC_31P25, // 11
    DRV8434_TRQ_DAC_25,    // 12
};

/*********************** REGISTER VALUES DEFINES ***********************/

#define DRV8434_REG_CTRL2_VAL_ENOUT_DISABLE     0x0u
#define DRV8434_REG_CTRL2_VAL_ENOUT_ENABLE      0x1u
#define DRV8434_REG_CTRL2_VAL_TOFF_16US         0x1u
#define DRV8434_REG_CTRL2_VAL_DECAY_SMARTRIPPLE 0x7u

#define DRV8434_REG_CTRL3_VAL_SPIDIR_PIN           0x0u
#define DRV8434_REG_CTRL3_VAL_SPISTEP_PIN          0x0u
#define DRV8434_REG_CTRL3_VAL_MICROSTEP_MODE_1_128 0x9u

#define DRV8434_REG_CTRL4_VAL_LOCK   0x6u
#define DRV8434_REG_CTRL4_VAL_UNLOCK 0x3u

#define DRV8434_REG_CTRL7_VAL_RCRIPPLE_1PERCENT 0x0u
#define DRV8434_REG_CTRL7_VAL_ENSSC_ENABLE      0x1u
#define DRV8434_REG_CTRL7_VAL_TRQSCALE_NOSCALE  0x0u
