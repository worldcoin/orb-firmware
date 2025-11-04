# Camera trigger and IR LED pulse control

This module is designed to control IR LEDs and camera exposures. One master timer controlling the frame rate (fps),
triggers the slave timers which control the pulse length, hence the duty cycle (%) and on-time/exposure (us) of all the
timer channels and outputs.

Values set through functions `ir_camera_system_set_fps()` and `ir_camera_system_set_on_time_us()` get checked for duty
cycle and on-time limits.

### Timer channels signal outputs:

    Pearl Orb Channels:
        IR LEDs: 850NM_LEFT, 850NM_RIGHT, 940NM_LEFT, 940NM_RIGHT
        Cameras: TOF_CAMERA, IR_EYE_CAMERA, IR_FACE_CAMERA

    Diamond Orb Channels:
        IR LEDs: 850NM_CENTER, 850NM_SIDE, 940NM_LEFT, 940NM_RIGHT, 940NM_SINGLE
        Cameras: TOF_CAMERA, IR_EYE_CAMERA, IR_FACE_CAMERA

### Configuration Limits:

See `IR_CAMERA_SYSTEM_MAX_IR_LED_ON_TIME_US`:

    Pearl Orb:
        Max Duty Cycle: 15%
        Max On-time: 5000us

    Diamond Orb:
        Max Duty Cycle: 25%
        Max On-time: 8000us (theorically), set by software to 7500us

## Timer Structure and Settings

The timer structure ensures synchronization of IR LEDs and camera triggers across all configurations:

    Master Timer (Timer 4):
        Pearl: Sets the frequency from 0 Hz to 60Hz (frame rate) to generate the primary trigger for all cameras.
        Diamond: Sets the delay until next IR LED & eye camera trigger; based on RGB-IR strobe signal falling edge,
                 to synchronize IR LED activation and camera exposures.

    850NM Timer (Timer 15):
        Pearl: Channels 1 (850nm left), 2 (850nm right)
        Diamond: Channels 1 (850nm center), 2 (850nm side)

    940NM Timer (Timer 3):
        Pearl: Channels 1 (940nm left), 4 (940nm right)
        Diamond: Channels 1 (940nm left), 4 (940nm right), 3 (940nm single)

    Camera Trigger Timer:
        Pearl: Timer 8 - Channels 1 (TOF), 3 (IR_EYE), 4 (IR_FACE)
        Diamond: Timer 20 - Channels 1 (TOF), 3 (IR_EYE), 4 (IR_FACE)

All slave timers are configured in single-shot mode, triggered by the master timer, with pulses ranging from 0us to the
max on-time of each configuration (5000us for Pearl, 8000us for Diamond). Timers operate with a prescaler such that 1
tick equals 1us, and are set in PWM mode 2 (output transitions from 0 to 1 t Compare-Capture events). The Capture
Compare Register (CCR) is set to the start delay value, and the Auto Reload Register (ARR) is configured to on_time_us +
start_delay.

On the Diamond Orb, the start delay of the camera triggers (CAMERA_TRIGGER_TIMER_START_DELAY_US) is configured as a
larger value than the start delay of the IR LEDs (IR_LED_TIMER_START_DELAY_US). This is done because the constant
current sources need approximately 50us to ramp up the LED current (measured 36 to 44us).

### Timer Configuration:

- Master Timer (Timer 4):
  - Trigger output at update event
  - ARR and Prescaler adjusted to generate the set frequency (frame rate)
- Other Timers (Timer 15, Timer 3, Timer 8/20):
  - Single shot mode without retrigger, trigger input from master timer
  - Prescaler set such that 1 tick equals 1us
  - CCR set to a fixed value defining an delay between cameras/IR-LEDs
  - ARR is adjusted to define the on-time/duty cycle of this output

**Timer Configuration Diagram**

    +-------------+    +---------------+
    |   Timer 4   |--->|   Timer 15    |----> 850NM LED Pulse
    | Master Timer|    |   850NM Timer |      Channels     1       2
    +-------------+    |               |       (Pearl:   Left,   Right)
            |          +---------------+       (Diamond: Center, Side )
            |
            |          +---------------+
            +--------->|   Timer 3     |----> 940NM LED Pulse
            |          |   940NM Timer |      Channels    1     4       3
            |          +---------------+       (Pearl:   Left, Right        )
            |                                  (Diamond: Left, Right, Single)
            |
            |          +-------------------+
            +--------->|   Timer 8 / 20    |----> Camera Triggers
                       | (Pearl / Diamond) |      Channels          1     3             4
                       +-------------------+       (Pearl/Diamond: TOF, IR_EYE, IR_FACE (pearl only))

### Timer cycle procedure

- Diamond only: when using the RGB/IR face camera, the master timer (Timer 4) is configured on strobe (input) falling
  edge interrupt to calculate the delay until the next strobe falling edge, minus the IR LED pulse length and a margin
  of 50us.
- Master Timer (Timer 4):
  - Shows only one complete frequency cycle to denote the master trigger.
- Other Timers (Timer 15, Timer 3, Timer 8/20):
  - Low Period (pre-CCR): Indicates the time before the pulse starts, depicted as a short interval.
  - Pulse: Immediately follows the CCR, output goes from low to high.
  - ARR: The total period from the start to the end of the pulse, output goes from high to low.

**Pearl**

    +---------------------------------------------------------------------------------+
    | Timing Diagram: Pearl Timers Configuration                                      |
    +---------------------------------------------------------------------------------+
    | Time ->                                                                         |
    |                                                                                 |
    |   Timer 4 (Master Timer)                                                        |
    |   ┌─────────────────────────────────────────────────────────────────────────>   |
    |   | Frequency Cycle (ARR based)                                                 |
    |   ├─────────────────────────────────────────────────────────────────────────>   |
    |   | Trigger                                                                     |
    |   |                                                                             |
    |  \|/                                                                            |
    |   *                                                                             |
    |   Timer 15 (850NM Timer)                                                        |
    |         ┌────────────────────────────────────┐                                  |
    |    _____|                 Pulse              |______________________________    |
    |   |<CCR>|                                                                       |
    |   |<----------------------ARR--------------->|                                  |
    |                                                                                 |
    |   Timer 3 (940NM Timer)                                                         |
    |         ┌────────────────────────────────────┐                                  |
    |    _____|                 Pulse              |______________________________    |
    |   |<CCR>|                                                                       |
    |   |<----------------------ARR--------------->|                                  |
    |                                                                                 |
    |   Timer 8 / 20 (Camera Trigger Timer)                                           |
    |             ┌────────────────────────────────┐                                  |
    |    _________|                 Pulse          |______________________________    |
    |   |<--CCR-->|                                                                   |
    |   |<----------------------ARR--------------->|                                  |
    |                                                                                 |
    +---------------------------------------------------------------------------------+

**Diamond**

    +---------------------------------------------------------------------------------+
    | Timing Diagram: Diamond Timers Configuration                                    |
    +---------------------------------------------------------------------------------+
    | Time ->                                                                         |
    |   RGB/IR face camera strobe signal                                              |
    |  ─────────┐                                        - - ───────────┐             |
    |    STROBE |                                             STROBE    |             |
    |           |                                                       |             |
    |           | Timer 4 (Master Timer), ARR update = 1 / FPS          |             |
    |           | The counter is updated so that the update event       |             |
    |           | triggers ir led pulse & eye cam capture to happen     |             |
    |           | before next strobe ends:                              |             |
    |           | = end of next strobe - IR pulse length - margin M     |             |
    |           *─────────────────────┐                                 |             |
    |                                 | Trigger                         |             |
    |                                 |                                 |             |
    |                                \|/                                |             |
    |                                 *                              <M>|             |
    |                                 Timer 15 (850NM Timer)            |             |
    |                                 |   ┌─────────────────────────┐   |             |
    |                                 |___|           Pulse         |___|____________ |
    |                                 |<CCR>|                           |             |
    |                                 |<------------ARR------------>|   |             |
    |                                 |                                 |             |
    |                                 |imer 3 (940NM Timer)             |             |
    |                                 |   ┌─────────────────────────┐   |             |
    |                                 |___|           Pulse         |___|____________ |
    |                                 |<CCR>|                           |             |
    |                                 |<------------ARR------------>|   |             |
    |                                 |                                 |             |
    |                                 |imer 8 / 20 (Camera Trigger Timer|             |
    |                                 |     ┌───────────────────────┐   |             |
    |                                 |_____|           Pulse       |___|____________ |
    |                                 |<CCR>|                           |             |
    |                                 |<------------ARR------------>|   |             |
    |                                                                                 |
    +---------------------------------------------------------------------------------+
