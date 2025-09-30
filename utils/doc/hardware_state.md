## Hardware State Reference

This document summarizes all hardware component states registered via `ORB_STATE_REGISTER` /
`ORB_STATE_REGISTER_MULTIPLE` and how they are set, to help support identify misbehaving components.

For each state:

- What hardware it refers to
- What condition/self-test sets the state
- Possible statuses and their meaning (messages shown in quotes when applicable)

### jetson

- **Hardware component**: Jetson power and boot signaling
- **How it’s evaluated**: MCU enables Jetson `POWER_EN`, waits for Jetson `SYS_RESET*` to assert within a timeout; also
  annotated while waiting-for-button-press flow
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED` – "orb turned off" or "booting...": Jetson not yet powered / boot sequence started
  - `RET_ERROR_INTERNAL` – "error reading reset pin %d": Failed to read reset GPIO
  - `RET_ERROR_TIMEOUT` – "timeout waiting for reset": Jetson did not assert reset in time
  - `RET_SUCCESS` – "booted": Jetson considered up and running

### pvcc

- **Hardware component**: IR LEDs supply availability (PVCC) on front unit
- **How it’s evaluated**: Read `front_unit_pvcc_enabled` GPIO (via expander). If low, eye-safety tripped, IR LED usage
  blocked
- **Statuses**:
  - `RET_SUCCESS` – "ir leds usable": PVCC enabled
  - `RET_ERROR_OFFLINE` – "ir leds unusable": PVCC disabled (eye-safety tripped)

### ir_safety

- **Hardware component**: IR eye-safety circuitry
- **How it’s evaluated**: Self-test toggles IR LED subsets and checks PVCC cutoff occurs within a short timeout
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED`: Front-unit PVCC sense input not configured/ready
  - `RET_ERROR_UNSAFE` – "%s didn't disable pvcc": A specific LED subset failed to trip safety
  - `RET_SUCCESS`: All LED lines tripped safety within timeout
- **Notes**:
  - This test is run very early at startup, before one is able to press the button to start the orb.
  - It's not performed until the next boot.

### voltages

- **Hardware component**: System power rails
- **How it’s evaluated**: ADC sampling across rails; periodic validation against configured min/max; self-test ensures
  stability over time;
- **Statuses**:
  - `RET_ERROR_INVALID_STATE` – "%u voltages not in range": One or more rails out of range
  - `RET_ERROR_TIMEOUT` – "self-test failed, %u iterations": Self-test didn’t stabilize within allowed iterations
  - `RET_SUCCESS` – "OK" or "self-test passed after %u iterations": All rails in range

### supercaps (diamond only)

- **Hardware component**: Supercapacitors
- **How it’s evaluated**: Muxed ADC sampling; compute differential voltages per cap, check range
- **Statuses**:
  - `RET_ERROR_INVALID_STATE` – "%d voltages out of range": One or more cap segments out of range
  - `RET_SUCCESS` – "voltages in range": All segments in range

### tof_1d

- **Hardware component**: VL53L1 1D ToF distance sensor
- **How it’s evaluated**: Periodic fetch and channel reads (distance and proximity). Consecutive errors increment an internal counter; thresholds drive state changes and recovery behavior.
- **Statuses**:
  - `RET_ERROR_INVALID_STATE` – "device not ready": Driver/device not ready
  - `RET_ERROR_INTERNAL` – "error reading": Fetch errors ≥3 consecutively
  - `RET_SUCCESS`: Normal periodic success (message omitted)
- **Notes**:
  - After ≥5 consecutive errors, the thread periodically re-initializes the sensor configuration until it succeeds, then clears the error counter and returns to `RET_SUCCESS`.

### sensor_bar (diamond only)

- **Hardware component**: 1D ToF sensor bar connectivity
- **How it’s evaluated**: Observed via 1D ToF consecutive error streaks (fetch or channel reads).
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED` – "unknown state": Default before sensor thread starts
  - `RET_ERROR_OFFLINE` – "disconnected?": 5 fetch errors consecutively
  - `RET_SUCCESS`: Healthy periodic operation (message omitted)

### polarizer (diamond only)

- **Hardware component**: Polarizer wheel (stepper, encoder, driver)
- **How it’s evaluated**: Auto-homing using encoder notches; driver/pwm/irq init; spin attempts and timeouts
- **Statuses**:
  - `RET_ERROR_INVALID_PARAM` – "invalid/NULL hw_version": Bad init argument
  - `RET_ERROR_NOT_INITIALIZED` – "no encoder: no wheel? staled?", "bumps not correctly detected", "init failed":
    Encoder or initialization issues
  - `<ret_code>` – "unable to spin": Spin operation failed; underlying return code indicates cause
  - `RET_SUCCESS` – "homed" / "init success": Homed or initialized correctly

### liquid_lens

- **Hardware component**: Liquid lens current driver and control loop
- **How it’s evaluated**: Self-test checks PWM response to current setpoint changes; init configures timers, GPIO, ADC
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED`: Default at init prior to self-test
  - `<ret_code>` – "self-test failed": Self-test failed (commonly assertion-based)
  - `RET_SUCCESS`: Self-test passed (message omitted)

### front_leds

- **Hardware component**: Front RGB LED strip (Diamond)
- **How it’s evaluated**: Self-test writes LED frames and checks two test GPIOs (DO/CO) for expected activity and
  return-to-idle; detects driver update errors
- **Statuses**:
  - `RET_ERROR_INVALID_STATE` – "led strip update err %d": Driver update failure
  - `RET_SUCCESS` – "front leds ok": Test passed
  - `RET_SUCCESS` – "ok, but dout/cout test signal stuck?": Lights okay but test lines didn’t return to idle as
    expected
  - `RET_ERROR_INVALID_STATE` – "led strip cut?": No valid toggles observed on test lines

### button_led

- **Hardware component**: Operator LEDs (diagnostic bit-bang path)
- **How it’s evaluated**: Bit-banged clock/data to LEDs; reads back test GPIO while toggling
- **Statuses**:
  - `RET_ERROR_INVALID_STATE` – "test failed": Readback didn’t behave as expected
  - `RET_SUCCESS` – "op leds ok (%zu)": Valid toggles observed
  - `RET_ERROR_INVALID_STATE` – "disconnected?": No toggles detected

### sound

- **Hardware component**: TAS5805M audio amplifier
- **How it’s evaluated**: Configure GPIO mux and I2C access; read DIE_ID; set digital volume and analog gain, verify
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED` – "comm issue (%d), die id: %d": I2C or identity mismatch
  - `RET_SUCCESS`: Amp configured and responsive (message omitted)

### nfc

- **Hardware component**: ST25R3918 NFC reader
- **How it’s evaluated**: HW version gating (feature reintroduced ≥ 4.6), I2C bus readiness, "set default" command,
  identity register matches expected type
- **Statuses**:
  - `RET_SUCCESS` – "no nfc on mainboard prior to 4.6": Not a failure; feature absent on earlier boards
  - `RET_ERROR_INVALID_STATE` – "I2C bus not ready", "SET_DEFAULT cmd failed (%d)": Bus/command failure
  - `RET_SUCCESS` – "identity ok: 0x%02x": Device identity verified
  - `RET_ERROR_ASSERT_FAILS` – "identity check failed: 0x%x (%d)": Identity unexpected or read error

### als

- **Hardware component**: Ambient light sensor
- **How it’s evaluated**: Periodic fetch/get with error counter threshold; HW version gating for front PCB; driver
  readiness
- **Statuses**:
  - `RET_ERROR_NOT_SUPPORTED` – "no als on that front pcb v: %u": No ALS on that front PCB revision
  - `RET_ERROR_NOT_INITIALIZED` – "als not ready (driver init failed?)": Device not ready
  - `RET_ERROR_INTERNAL` – "sensor fetch: ret: %d", "sensor get: ret: %d": Repeated runtime errors
  - `RET_SUCCESS`: Normal operation or recovery after transient errors (message omitted)

### fan_tach

- **Hardware component**: Fan tachometer (main fan RPM)
- **How it’s evaluated**: GPIO edge interrupts tally RPM; separate self-test (coarse) ensures RPM increases when
  commanded
- **Statuses**:
  - `<err_code>` – "init failed": Tach GPIO/IRQ setup failed
  - `RET_SUCCESS`: Initialized or self-test pass
  - `RET_ERROR_ASSERT_FAILS` – "speed didn't increase", "fan not running": Self-test failure

### tmp_main / tmp_front / tmp_lens

- **Hardware component**: Temperature sensors for main board, front unit (or averaged), and liquid lens
- **How it’s evaluated**: Periodic sampling; device readiness; over-temperature thresholds with hysteresis and critical
  timer; timeouts on sampling failure
- **Statuses**:
  - `RET_ERROR_NOT_INITIALIZED` – "not ready": Sensor driver not ready
  - `RET_ERROR_TIMEOUT` – "failed to sample, last ret %d": Sampling repeatedly failed
  - `RET_ERROR_UNSAFE` – "over temperature", "over threshold: X>YºC": Over-temperature condition (also drives fan
    emergency mode)
  - `RET_SUCCESS` – "initialized", "nominal" or empty: Sensor OK / recovered

## Status Codes Reference

- `RET_SUCCESS`: Test/condition passed or nominal
- `RET_ERROR_NOT_INITIALIZED`: Device or state isn’t initialized/ready
- `RET_ERROR_INVALID_STATE`: Unexpected/unhealthy runtime state
- `RET_ERROR_OFFLINE`: Device likely disconnected/offline
- `RET_ERROR_INTERNAL`: Internal I/O/processing error
- `RET_ERROR_TIMEOUT`: Timed out waiting for a condition/data
- `RET_ERROR_ASSERT_FAILS`: Self-test/assertion failed
- `RET_ERROR_ALREADY_INITIALIZED`: Already set up (non-fatal in context)
- `RET_ERROR_NOT_SUPPORTED`: Not supported on this hardware
- `RET_ERROR_UNSAFE`: Safety condition violated (e.g., over-temperature)
