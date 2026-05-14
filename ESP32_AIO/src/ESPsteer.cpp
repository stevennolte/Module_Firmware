/**
 * @file ESPsteer.cpp
 * @brief Implementation of precision steering control system
 * 
 * @details This file implements the ESPsteer class which provides comprehensive
 *          steering control for precision agriculture applications including:
 *          - PID-based closed-loop steering control with auto-tuning
 *          - Real-time wheel angle feedback processing
 *          - Motor driver control with current monitoring
 *          - Multi-threaded operation for real-time performance
 *          - Safety systems and emergency override functionality
 *          - Configurable steering parameters and calibration
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPsteer.h for class interface definition
 * @see MotorDriver.h for motor control functionality
 * @see WAS.h for wheel angle sensor interface
 */

#include "ESPsteer.h"

// #define PRINT_PID_DEBUG

//TODO: SET STEER ANGLE, IMU DATA, ETC DURING TIMEOUT

/**
 * @brief Constructor for steering control system
 * 
 * @param vars Pointer to ESPdata singleton for configuration and data storage
 * @param ads Pointer to ADS1115 ADC for analog sensor readings
 * 
 * @details Initializes steering control system with motor driver, wheel angle sensor,
 *          PID controller, and MCP manager integration. Sets up the complete steering
 *          control architecture for precision guidance applications.
 */
ESPsteer::ESPsteer(ESPdata* vars) 
    : motorDriver(vars), was(vars), pid(-1.0,1.0, TuningMethod::Manual), i2cManager(I2CManager::getInstance()), currentLimitLatched(false) {
    espData = vars;
}

/**
 * @brief Main steering control loop running in dedicated FreeRTOS task
 * 
 * @details Continuously processes steering control operations including:
 *          - Configuration updates and PID parameter adjustments
 *          - Wheel angle sensor data processing
 *          - Motor current monitoring for load detection
 *          - Test state monitoring for manual override
 *          - Steering control mode selection (test vs automatic)
 *          - Real-time steering command processing
 * 
 *          Runs in a dedicated task with 3ms cycle time for responsive control.
 * 
 * @note This function runs indefinitely in a FreeRTOS task context
 * @see steerLoop(), steerTestLoop(), taskHandler()
 */
void ESPsteer::continuousLoop() {
    while (true) {
        vTaskDelay(3);
        espData->steer.looptime = millis() - espData->steer.looptimestamp;
        espData->steer.looptimestamp = millis();
        // vTaskDelay(3);
        if (espData->steer.settingsUpdated == 1){
            setPIDgains();
            if (espData->steer.set0 == 1){
                // TODO: Set steer angle offset

                espData->steer.set0 = 0;
            }
            espData->steer.settingsUpdated = 0;
        }
        was.loop();
        espData->steer.steerCurrent = getCurrent();
        
        // Current limit check - LATCHED mode: once tripped, stays disabled until AgOpenGPS re-enables
        static uint8_t lastStatus = 0;  // Track previous status for edge detection
        static bool enableResetPending = false;  // Remember enable request if current is still high
        
        if (espData->steer.enableCurrentLimit) {
            if (espData->steer.steerCurrent >= espData->steer.currentLimit) {
                if (!currentLimitLatched) {
                    // First time exceeding limit - latch the fault
                    currentLimitLatched = true;
                    espData->steer.currentLimitTripped = true;
                    Serial.println("*** CURRENT LIMIT EXCEEDED - LATCHED ***");
                    Serial.printf("Current: %d >= Limit: %d\n", 
                                  espData->steer.steerCurrent, espData->steer.currentLimit);
                    Serial.println("*** Steering DISABLED - Operator must press ENABLE in AgOpenGPS to clear ***");
                }
            }
            
            // Capture enable request on rising edge (operator pressed ENABLE in AgOpenGPS).
            if (espData->steer.status == 1 && lastStatus == 0 && currentLimitLatched) {
                enableResetPending = true;
                if (espData->steer.steerCurrent < espData->steer.currentLimit) {
                    currentLimitLatched = false;
                    espData->steer.currentLimitTripped = false;
                    enableResetPending = false;
                    Serial.println("*** CURRENT LIMIT LATCH CLEARED - AgOpenGPS enable command received ***");
                } else {
                    Serial.println("*** ENABLE QUEUED - Waiting for current to drop below threshold ***");
                }
            }

            // If enable was requested while current was high, clear automatically once safe.
            if (currentLimitLatched && enableResetPending &&
                espData->steer.status == 1 &&
                espData->steer.steerCurrent < espData->steer.currentLimit) {
                currentLimitLatched = false;
                espData->steer.currentLimitTripped = false;
                enableResetPending = false;
                Serial.println("*** CURRENT LIMIT LATCH CLEARED - queued ENABLE applied at safe current ***");
            }

            // If operator turns steering back off, require a fresh enable press later.
            if (espData->steer.status == 0) {
                enableResetPending = false;
            }
            
            // Keep sending the disabled state while latched
            espData->steer.currentLimitTripped = currentLimitLatched;
        } else {
            // Current limit monitoring disabled - clear latch
            if (currentLimitLatched) {
                currentLimitLatched = false;
                espData->steer.currentLimitTripped = false;
                enableResetPending = false;
                Serial.println("*** CURRENT LIMIT LATCH RESET - Current monitoring disabled/re-enabled ***");
            }
        }
        
        // Update last status for next iteration
        lastStatus = espData->steer.status;
        
        espData->steer.testState = getTestState();
        // Serial.println(espData->steer.testState);
        // vTaskDelay(1000);
        if (millis() - espData->steer.lastSteerOutMsgTime > espData->steer.steerMsgRate) {

            espData->steer.lastSteerOutMsgTime = millis();
            uint8_t testdata[14];
            testdata[0] = 0x80;
            testdata[1] = 0x81;
            testdata[2] = 0x7F;  // MajorPGN: Steer module
            testdata[3] = 0xFD;  // MinorPGN: PGN 253
            testdata[4] = 8;
            int16_t steerAngleSend = static_cast<int16_t>(espData->steer.actSteerAngle * 100);
            testdata[5] = static_cast<uint8_t>(steerAngleSend & 0xFF);
            testdata[6] = static_cast<uint8_t>((steerAngleSend >> 8) & 0xFF);
            testdata[7] = 9999 & 0xFF;
            testdata[8] = 9999 >> 8;
            testdata[9] = 8888 & 0xFF;
            testdata[10] = 8888 >> 8;
            
            // Byte 11: Switch status byte - FLIPPED bit assignment
            // Bit 0 = work switch (sections - joystick switchStates[6]): 1=ENABLED, 0=DISABLED
            // Bit 1 = steer switch: 0=ENABLED, 1=DISABLED (set when current limit tripped)
            // Bit 2 = remote switch (joystick switchStates[7])
            testdata[11] = 0x00; // Initialize to 0 (bit 1 clear = steering enabled by default)
            
            // Bit 0: Work switch (section control) from joystick
            if (espData->joystick.switchStates[6] == 1) {
                testdata[11] |= 0x01;
            }
            
            // Bit 1: Steer switch - set when current limit trips to disable steering
            // This tells AgOpenGPS to disable steering due to overcurrent protection
            if (espData->steer.currentLimitTripped) {
                testdata[11] |= 0x02; // Set bit 1 = steering DISABLED
            }
            // Normal operation: bit 1 = 0 = steering ENABLED
            
            // Bit 2: Remote switch from joystick  
            if (espData->joystick.switchStates[7] == 1) {
                testdata[11] |= 0x04;
            }
            
            // Debug output - always show when current limit is enabled for monitoring
            if (espData->steer.enableCurrentLimit || espData->steer.enableCurrentDebug) {
                static uint8_t lastByte11 = 0xFF;
                if (testdata[11] != lastByte11) {
                    Serial.printf("[PGN253] Byte11 CHANGED: 0x%02X | CurrentLimit: Tripped=%d Value=%d/%d | SteerSwitch=%s\n",
                                  testdata[11], espData->steer.currentLimitTripped,
                                  espData->steer.steerCurrent, espData->steer.currentLimit,
                                  (testdata[11] & 0x02) ? "DISABLED" : "ENABLED");
                    lastByte11 = testdata[11];
                }
            }
            
            testdata[12] = static_cast<uint8_t>((espData->steer.pwmCmd * 255) / 8109);
            testdata[13] = espUdp->calcChecksum(testdata, sizeof(testdata));
            espUdp->udp.writeTo(testdata, sizeof(testdata), IPAddress(espData->wifi.ips[0], espData->wifi.ips[1], espData->wifi.ips[2], 255), 9999);
            
            // espUdp->sendUDP(testdata, sizeof(testdata));
            vTaskDelay(1);  // Reduced from 10ms to 1ms for consistent loop timing
            // Current Message
            uint8_t currentData[14];
            currentData[0] = 0x80;
            currentData[1] = 0x81;
            currentData[2] = espData->wifi.ips[3];
            currentData[3] = 250;
            currentData[4] = 8;
            currentData[5] = espData->steer.steerCurrent; // Already scaled to 1-254 range
            currentData[13] = espUdp->calcChecksum(currentData, sizeof(currentData));
            espUdp->udp.writeTo(currentData, sizeof(currentData), IPAddress(espData->wifi.ips[0], espData->wifi.ips[1], espData->wifi.ips[2], 255), 9999);
        }


        switch (espData->steer.status){
            case 1:
                steerLoop();
                break;
            case 0:
                // if (espData->steer.testState != 0){
                steerTestLoop();
                // }
                break;
        }
        
    }
}

void ESPsteer::steerTestLoop(){
    // Serial.println("Steer Test Loop");
    switch (espData->steer.testState){
        case 1:
            if (pid.getSetpoint() != 1){
                pid.setSetpoint(1.0);
            }
            // pid.setSetpoint(1.0);
            pid.update(0);
            motorDriver.setOutput(pid.getOutput());
            
            
            break;
        case 2:
            pid.setSetpoint(-1);
            pid.update(0);
            motorDriver.setOutput(pid.getOutput());
            break;
        default:
            pid.setSetpoint(0);
            pid.update(0);
            motorDriver.setOutput(pid.getOutput());
            break;
    }
    #ifdef PRINT_PID_DEBUG
    Serial.print("Test State: ");
    Serial.print(espData->steer.testState);
    Serial.print("\tSetpoint: ");
    Serial.print(pid.getSetpoint());
    Serial.print("\tOutput: ");
    Serial.println(pid.getOutput());
    #endif
}

void ESPsteer::steerLoop(){

    // Check watchdog timeout
    if (millis() - espData->steer.watchdog > 2000){
        espData->steer.status = 0;
    }
    
    if (espData->steer.status == 1){
        float angleError = espData->steer.targetSteerAngle - espData->steer.actSteerAngle;
        if (espData->steer.steerDeadband > 0.0f && fabsf(angleError) <= espData->steer.steerDeadband) {
            // Within deadband: set motor to 0. Setpoint is held at actual angle so the
            // PID sees zero error and the integral term does not accumulate (windup prevention).
            pid.setSetpoint(espData->steer.actSteerAngle);
            pid.update(espData->steer.actSteerAngle);
            espData->steer.pidCmd = 0.0f;
            motorDriver.setOutput(0.0f);
            // Reset stiction state when inside deadband
            _stictionActive = false;
            _stictionStartTime = 0;
        } else {
            // Gain scheduling: optionally switch Kp based on error magnitude
            if (espData->steer.enableGainSchedule) {
                bool nearZone = fabsf(angleError) < espData->steer.gainScheduleThreshold;
                if (nearZone != _gainScheduleNearZone) {
                    _gainScheduleNearZone = nearZone;
                    float kpOverride = nearZone ? espData->steer.gainPNear : espData->steer.gainPFar;
                    if (kpOverride > 0.0f) {
                        pid.setManualGains(kpOverride, espData->steer.gainI, espData->steer.gainD);
                    } else {
                        setPIDgains();
                    }
                }
            } else if (_gainScheduleNearZone) {
                // Gain scheduling was just disabled – restore base gains once
                _gainScheduleNearZone = false;
                setPIDgains();
            }

            pid.setSetpoint(espData->steer.targetSteerAngle);
            pid.update(espData->steer.actSteerAngle);
            float pidOut = pid.getOutput();

            // Stiction boost: detect stall and add a temporary extra output
            if (espData->steer.stictionBoost > 0.0f) {
                float moved = fabsf(espData->steer.actSteerAngle - _stictionLastAngle);
                if (moved >= espData->steer.stictionThreshold) {
                    // Wheel is moving – reset stall timer
                    _stictionStartTime = millis();
                    _stictionLastAngle = espData->steer.actSteerAngle;
                    _stictionActive = false;
                } else if (_stictionStartTime == 0) {
                    _stictionStartTime = millis();
                    _stictionLastAngle = espData->steer.actSteerAngle;
                } else if (millis() - _stictionStartTime >= espData->steer.stictionTimeout) {
                    _stictionActive = true;
                }
                if (_stictionActive) {
                    float boost = (angleError > 0.0f)
                        ? espData->steer.stictionBoost
                        : -espData->steer.stictionBoost;
                    pidOut = constrain(pidOut + boost, -1.0f, 1.0f);
                }
            } else {
                // Stiction disabled – clear state
                _stictionActive = false;
                _stictionStartTime = 0;
            }

            espData->steer.pidCmd = pidOut;
            motorDriver.setOutput(pidOut);
        }
    } else {
        pid.setSetpoint(0);
        pid.update(0);
        espData->steer.pidCmd = pid.getOutput();
        motorDriver.setOutput(espData->steer.pidCmd);
        _stictionActive = false;
        _stictionStartTime = 0;
    }
}


void ESPsteer::taskHandler(void *param) {
    ESPsteer* instance = (ESPsteer*)param;
    instance->continuousLoop();
}

void ESPsteer::setPIDgains(){
    pid.setManualGains(float(espData->steer.gainP)/espData->steer.gainPScalar, espData->steer.gainI, espData->steer.gainD);
}

void ESPsteer::begin(ESPudp* espUdp) {
    Serial.println("Initializing Steering System...");
    this->espUdp = espUdp;
    pinMode(espData->pins.STEER_TEST_PIN, INPUT);
    _status = espData->steer.status;
    

    motorDriver.init();
    Serial.println("\tMotor Driver Initialized");
    was.init();

    // *********Start PID Setup**********
    Serial.println("\tSetting PID Gains");
    setPIDgains();
    pid.setSetpoint(0); // Target setpoint
    if (espData->steer.pidInputFilt > 0.0f) {
        pid.enableInputFilter(espData->steer.pidInputFilt);
    }
    if (espData->steer.enableAntiWindup) {
        pid.enableAntiWindup(true, espData->steer.antiWindupThreshold);
    }
    // ********End PID Setup**********

    Serial.println("\tWAS Initialized");
    xTaskCreatePinnedToCore(taskHandler, "Steer_Task", 10000, this, 3, NULL, 0);
}

uint32_t ESPsteer::getCurrent() {
    // Read motor current from ADS1115 channel (configured in ESPdata)
    uint16_t rawReading = i2cManager.getRawReading(espData->adsConfig.motorCurrentChannel);
    
    // Store raw reading for debug
    espData->steer.rawCurrentADC = rawReading;

    // ADS1115 returns a signed value; negative readings (noise below zero-current)
    // must be clamped to 0 before scaling to prevent uint16_t wrap-around overflow.
    int16_t signedReading = static_cast<int16_t>(rawReading);
    uint16_t clampedReading = (signedReading < 0) ? 0 : static_cast<uint16_t>(signedReading);

    // Simple linear scaling: map 16-bit ADC (0-65535) to 1-254 range
    // Using map function: scaledValue = (rawADC * 253) / 65535 + 1
    uint32_t scaledCurrent = map(clampedReading, 0, 65535, 1, 254);
    
    // Apply scaler multiplier and constrain to 1-254 range
    scaledCurrent = constrain((uint32_t)(scaledCurrent * espData->steer.currentScaler), 1, 254);
    
    // Store scaled value for debug
    espData->steer.currentBeforeFilter = scaledCurrent;
    
    // Apply exponential moving average filter.
    // currentFilter is the weight given to the new sample; (1 - currentFilter) weights the old value.
    filteredCurrent = (filteredCurrent * (1.0f - espData->steer.currentFilter))
                    + (scaledCurrent   * espData->steer.currentFilter);
    scaledCurrent = constrain((uint32_t)roundf(filteredCurrent), 1, 254);
    
    // Debug output if enabled
    if (espData->steer.enableCurrentDebug) {
        static uint32_t lastDebugTime = 0;
        if (millis() - lastDebugTime > 500) {  // Print every 500ms to avoid spam
            lastDebugTime = millis();
            Serial.println("=== STEER CURRENT DEBUG ===");
            Serial.printf("  Raw ADC:          %d\n", rawReading);
            Serial.printf("  Scaler:           %.2f\n", espData->steer.currentScaler);
            Serial.printf("  Before Filter:    %d (1-254 range)\n", (int)espData->steer.currentBeforeFilter);
            Serial.printf("  Filter:           new=%.2f old=%.2f\n", espData->steer.currentFilter, 1.0f - espData->steer.currentFilter);
            Serial.printf("  Filtered Current: %d (1-254 range)\n", scaledCurrent);
            Serial.printf("  Limit Enabled:  %s\n", espData->steer.enableCurrentLimit ? "YES" : "NO");
            Serial.printf("  Current Limit:  %d\n", espData->steer.currentLimit);
            Serial.printf("  Limit Tripped:  %s\n", espData->steer.currentLimitTripped ? "YES" : "NO");
            Serial.printf("  PWM Command:    %d\n", espData->steer.pwmCmd);
            Serial.printf("  Motor Dir:      %d (0=stop, 1=CW, 2=CCW)\n", espData->steer.motorDirection);
            Serial.printf("  Min/Max PWM:    %d / %d\n", espData->steer.minPWM, espData->steer.highPWM);
            Serial.println("==========================");
        }
    }
    
    return scaledCurrent;
}

uint8_t ESPsteer::getTestState(){
    uint32_t reading = analogReadMilliVolts(espData->pins.STEER_TEST_PIN);
    if (reading < 700){
        return 1;
    } else if (reading > 3000){
        return 2;
    } 
    return 0;
}
