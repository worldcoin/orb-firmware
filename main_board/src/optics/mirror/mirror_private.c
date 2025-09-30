#include "mirror_private.h"

#include "errors.h"

#include <app_assert.h>
#include <math.h>
#include <utils.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/byteorder.h>

#if defined(CONFIG_BOARD_PEARL_MAIN)

#elif defined(CONFIG_BOARD_DIAMOND_MAIN)
// values measured on first PoC Diamond Orb

#endif

const uint8_t TMC5041_REGISTERS[REG_IDX_COUNT][MOTORS_COUNT] = {
    {0x20, 0x40}, // RAMPMODE
    {0x21, 0x41}, // XACTUAL
    {0x22, 0x42}, // VACTUAL
    {0x23, 0x43}, // VSTART
    {0x27, 0x47}, // VMAX
    {0x2D, 0x4D}, // XTARGET
    {0x30, 0x50}, // IHOLD_IRUN
    {0x34, 0x54}, // SW_MODE
    {0x35, 0x55}, // RAMP_STAT
    {0x6D, 0x7D}, // COOLCONF
    {0x6F, 0x7F}, // DRV_STATUS
};

const uint64_t position_mode_full_speed[MOTORS_COUNT][10] = {
    [0] =
        {
            0xEC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                          // (spreadCycle)
            0xB000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
            0xA400008000, // A1 first acceleration
            0xA500000000 +
                MOTOR_FS_VMAX * 3 / 4, // V1 Acceleration threshold, velocity V1
            0xA600001000,              // Acceleration above V1
            0xA700000000 + MOTOR_FS_VMAX, // VMAX
            0xA800001000,                 // DMAX Deceleration above V1
            0xAA00008000,                 // D1 Deceleration below V1
            0xAB00000010,                 // VSTOP stop velocity
            0xA000000000,                 // RAMPMODE = 0 position move
        },
    [1] = {
        0xFC000100C5, // CHOPCONF TOFF=5, HSTRT=4, HEND=1, TBL=2, CHM=0
                      // (spreadCycle)
        0xD000011000, // IHOLD_IRUN reg, bytes: [IHOLDDELAY|IRUN|IHOLD]
        0xC400008000, // A1 first acceleration
        0xC500000000 +
            MOTOR_FS_VMAX * 3 / 4,    // V1 Acceleration threshold, velocity V1
        0xC600001000,                 // Acceleration above V1
        0xC700000000 + MOTOR_FS_VMAX, // VMAX
        0xC800001000,                 // DMAX Deceleration above V1
        0xCA00008000,                 // D1 Deceleration below V1
        0xCB00000010,                 // VSTOP stop velocity
        0xC000000000,                 // RAMPMODE = 0 position move
    }};

const int32_t mirror_center_angles[MOTORS_COUNT] = {
    [MOTOR_THETA_ANGLE] = MIRROR_ANGLE_THETA_CENTER_MILLIDEGREES,
    [MOTOR_PHI_ANGLE] = MIRROR_ANGLE_PHI_CENTER_MILLIDEGREES};

const double microsteps_per_mm = 12800.0; // 1mm / 0.4mm (pitch) * (360° / 18°
// (per step)) * 256 micro-steps

// SPI w/ TMC5041
#define SPI_DEVICE          DT_NODELABEL(motion_controller)
#define SPI_CONTROLLER_NODE DT_PARENT(SPI_DEVICE)

#define READ  0
#define WRITE (1 << 7)

static const struct spi_dt_spec spi_bus_dt = SPI_DT_SPEC_GET(
    SPI_DEVICE,
    SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA, 2);

static struct spi_buf rx;
static struct spi_buf_set rx_bufs = {.buffers = &rx, .count = 1};

static struct spi_buf tx;
static struct spi_buf_set tx_bufs = {.buffers = &tx, .count = 1};

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

void
motor_controller_spi_send_commands(const uint64_t *cmds, size_t num_cmds)
{
    uint64_t cmd;
    uint8_t tx_buffer[5];
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    for (size_t i = 0; i < num_cmds; ++i) {
        cmd = sys_cpu_to_be64(cmds[i] << 24);
        memcpy(tx_buffer, &cmd, 5);
        spi_write_dt(&spi_bus_dt, &tx_bufs);
    }
}

int
motor_controller_spi_write(uint8_t reg, int32_t value)
{
    uint8_t tx_buffer[5] = {0};
    uint8_t rx_buffer[5] = {0};

    // make sure there is the write flag
    reg |= WRITE;
    tx_buffer[0] = reg;

    tx_buffer[1] = (value >> 24) & 0xFF;
    tx_buffer[2] = (value >> 16) & 0xFF;
    tx_buffer[3] = (value >> 8) & 0xFF;
    tx_buffer[4] = (value >> 0) & 0xFF;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    int ret = spi_transceive_dt(&spi_bus_dt, &tx_bufs, &rx_bufs);
    ASSERT_SOFT(ret);

    return RET_SUCCESS;
}

uint32_t
motor_controller_spi_read(uint8_t reg)
{
    uint8_t tx_buffer[5] = {0};
    uint8_t rx_buffer[5] = {0};
    int ret;

    // make sure there is the read flag (msb is 0)
    reg &= ~WRITE;
    tx_buffer[0] = reg;

    rx.buf = rx_buffer;
    rx.len = sizeof rx_buffer;
    tx.buf = tx_buffer;
    tx.len = sizeof tx_buffer;

    // reading happens in two SPI operations:
    //  - first, send the register address, returned data is
    //    the one from previous read operation.
    //  - second, read the actual data

    // first, send reg address
    ret = spi_transceive_dt(&spi_bus_dt, &tx_bufs, &rx_bufs);
    ASSERT_SOFT(ret);

    memset(rx_buffer, 0, sizeof(rx_buffer));

    // second, read data
    ret = spi_transceive_dt(&spi_bus_dt, &tx_bufs, &rx_bufs);
    ASSERT_SOFT(ret);

    const uint32_t read_value = (rx_buffer[1] << 24 | rx_buffer[2] << 16 |
                                 rx_buffer[3] << 8 | rx_buffer[4] << 0);

    return read_value;
}

bool
motor_spi_ready()
{
    return device_is_ready(spi_bus_dt.bus);
}
