# Module Firmware - Discussed Items and Implementation Plan

This README captures the items and implementation plan discussed for improving ESP32_AIO steering behavior.

## Scope

Primary target:
- `ESP32_AIO/src/ESPsteer.cpp`
- `ESP32_AIO/src/MotorDriver.cpp`
- `ESP32_AIO/include/ESPdata.h`

## Discussed Items

1. Add stiction compensation so steering can break free under high resistance.
2. Use error-dependent minimum PWM to reduce near-target overshoot.
3. Enable anti-windup behavior for better recovery after large corrections.
4. Enable derivative/input filtering to reduce oscillation/noise response.
5. Keep deadband-based zero-output behavior near target.
6. Optionally add gain scheduling by error magnitude as a follow-up.

## Implementation Plan

### 1) Stiction Boost
- Add configuration fields in `ESPdata::Steer`:
  - `stictionBoost`
  - `stictionTimeout`
  - `stictionThreshold`
- In `ESPsteer::steerLoop()`, detect stalled movement while error remains outside deadband.
- Apply temporary output boost and clear it immediately once movement is detected.

### 2) Error-Dependent Min PWM
- Add configuration fields in `ESPdata::Steer`:
  - `minPWMnear`
  - `minPWMfar`
  - `minPWMFarThreshold`
- Compute effective minimum output from current angle error.
- Use that effective minimum in motor output scaling to reduce low-error overshoot.

### 3) Anti-Windup Activation
- Enable existing PID anti-windup in steering initialization.
- Add configurable anti-windup threshold field:
  - `antiWindupThreshold`

### 4) Input Filtering for Derivative Stability
- Wire `pidInputFilt` into PID input filter activation during initialization.
- Only enable when configured value is above zero.

### 5) Deadband Preservation
- Retain current deadband logic that zeros motor output inside threshold.
- Keep setpoint-hold behavior in deadband to limit integral buildup.

### 6) Optional Phase 2: Gain Scheduling
- Add optional near/far Kp configuration:
  - `gainPNear`
  - `gainPFar`
  - `gainScheduleThreshold`
- Switch gains based on absolute steering error.

## Suggested Tuning Sequence

1. Tune proportional behavior first (`gainI = 0`, `gainD = 0`).
2. Enable input filtering and modest derivative damping.
3. Introduce integral gain to remove steady-state error.
4. Tune stiction boost and timeout for heavy-load steering.
5. Adjust anti-windup threshold if large-move overshoot appears.

## Validation Notes

- Attempted pre-change module build command:
  - `cd /home/runner/work/Module_Firmware/Module_Firmware/ESP32_AIO && pio run`
- Result: failed in this environment because `pio` is not installed (`pio: command not found`).
