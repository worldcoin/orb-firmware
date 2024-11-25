/******************************************************************************
 * @file drv8434_registers.h
 * @brief Register map definitions for Texas Instruments DRV8434 stepper motor
 *driver
 *
 * This file contains the register addresses and bit definitions for the DRV8434
 * stepper motor driver IC. The DRV8434 is a dual H-bridge motor driver with
 * integrated current sensing, microstepping indexer, and protection features.
 *
 *
 * @note All register addresses and bit definitions are based on
 *       DRV8434 datasheet SLOSE70 – DECEMBER 2020
 *
 * @author Srikar Chintapalli
 *
 ******************************************************************************/

#define DRV8434_REG_FAULT_ADDR       0x00
#define DRV8434_REG_DIAGSTATUS1_ADDR 0x01
#define DRV8434_REG_DIAGSTATUS2_ADDR 0x02
#define DRV8434_REG_CTRL1_ADDR       0x03
#define DRV8434_REG_CTRL2_ADDR       0x04
#define DRV8434_REG_CTRL3_ADDR       0x05
#define DRV8434_REG_CTRL4_ADDR       0x06
#define DRV8434_REG_CTRL5_ADDR       0x07
#define DRV8434_REG_CTRL6_ADDR       0x08
#define DRV8434_REG_CTRL7_ADDR       0x09
#define DRV8434_REG_CTRL8_ADDR       0x0A
#define DRV8434_REG_CTRL9_ADDR       0x0B

// Fault Status Register

typedef union {
    uint8_t raw;
    struct {
        bool OL : 1;
        bool TF : 1;
        bool STL : 1;
        bool OCP : 1;
        bool CPUV : 1;
        bool UVLO : 1;
        bool SPI_ERROR : 1;
        bool FAULT : 1;
    };
} DRV8434_FAULT_REG_t;

// Diag Status Registers

typedef union {
    uint8_t raw;
    struct {
        bool OCP_HS1_A : 1;
        bool OCP_LS1_A : 1;
        bool OCP_HS2_A : 1;
        bool OCP_LS2_A : 1;
        bool OCP_HS1_B : 1;
        bool OCP_LS1_B : 1;
        bool OCP_HS2_B : 1;
        bool OCP_LS2_B : 1;
    };
} DRV8434_DIAGSTATUS1_REG_t;

typedef union {
    uint8_t raw;
    struct {
        bool OL_A : 1;
        bool OL_B : 1;
        bool RSVD_1 : 1;
        bool STALL : 1;
        bool STL_LRN_OK : 1;
        bool OTS : 1;
        bool OTW : 1;
        bool RSVD_2 : 1;
    };
} DRV8434_DIAGSTATUS2_REG_t;

// Control Registers

typedef union {
    uint8_t raw;
    struct {
        bool RSVD_1 : 1;
        bool OL_MODE : 1;
        uint8_t RSVD_2 : 2;
        uint8_t TRQ_DAC : 4;
    };
} DRV8434_CTRL1_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t DECAY : 3;
        uint8_t TOFF : 2;
        uint8_t RSVD_1 : 2;
        bool EN_OUT : 1;
    };
} DRV8434_CTRL2_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t MICROSTEP_MODE : 4;
        bool SPI_STEP : 1;
        bool SPI_DIR : 1;
        bool STEP : 1;
        bool DIR : 1;
    };
} DRV8434_CTRL3_REG_t;

typedef union {
    uint8_t raw;
    struct {
        bool TW_REP : 1;
        bool OTSD_MODE : 1;
        uint8_t OCP_MODE : 3;
        bool EN_OL : 1;
        bool LOCK : 1;
        bool CLR_FLT : 1;
    };
} DRV8434_CTRL4_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t RSVD_1 : 3;
        bool STL_REP : 1;
        bool EN_STL : 1;
        bool STL_LRN : 1;
        uint8_t RSVD_2 : 2;
    };
} DRV8434_CTRL5_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t STALL_TH : 8;
    };
} DRV8434_CTRL6_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t STALL_TH : 4;
        bool TRQ_SCALE : 1;
        bool EN_SSC : 1;
        uint8_t RC_RIPPLE : 2;
    };
} DRV8434_CTRL7_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t TRQ_COUNT : 8;
    };
} DRV8434_CTRL8_REG_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t TRQ_COUNT : 4;
        uint8_t REV_ID : 4;
    };
} DRV8434_CTRL9_REG_t;
