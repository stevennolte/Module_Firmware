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
    : motorDriver(vars), was(vars), pid(-1.0,1.0, TuningMethod::Manual), i2cManager(I2CManager::getInstance()) {
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
        espData->steer.looptime = millis() - espData->steer.looptimestamp;
        espData->steer.looptimestamp = millis();
        vTaskDelay(3);
        if (espData->steer.settingsUpdated == 1){
            setPIDgains();
            if (espData->steer.set0 == 1){
                // TODO: Set steer angle offset

                espData->steer.set0 = 0;
            }
            espData->steer.settingsUpdated = 0;
        }
        was.loop();
        // espData->steer.steerCurrent = getCurrent();
        espData->steer.testState = getTestState();
        // Serial.println(espData->steer.testState);
        // vTaskDelay(1000);
        if (millis() - espData->steer.lastSteerOutMsgTime > espData->steer.steerMsgRate) {

            espData->steer.lastSteerOutMsgTime = millis();
            uint8_t testdata[14];
            testdata[0] = 0x80;
            testdata[1] = 0x81;
            testdata[2] = espData->wifi.ips[3];
            testdata[3] = 253;
            testdata[4] = 8;
            testdata[5] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) & 0xFF;
            testdata[6] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) >> 8;
            testdata[7] = 9999 & 0xFF;
            testdata[8] = 9999 >> 8;
            testdata[9] = 8888 & 0xFF;
            testdata[10] = 8888 >> 8;
            if (espData->joystick.switchStates[6] == 1) {
                testdata[11] |= 0x01; // Set the first bit to 1
            } else {
                testdata[11] &= ~0x01; // Clear the first bit to 0
            }
                        
            // Set the second bit of testdata[11] to switchStates[7]
            if (espData->joystick.switchStates[7] == 1) {
                testdata[11] |= 0x02; // Set the second bit to 1
            } else {
                testdata[11] &= ~0x02; // Clear the second bit to 0
            }
            // testdata[11] = 0;
            testdata[12] = static_cast<uint8_t>((espData->steer.pwmCmd * 255) / 8109);
            testdata[13] = espUdp->calcChecksum(testdata, sizeof(testdata));
            espUdp->udp.writeTo(testdata, sizeof(testdata), IPAddress(espData->wifi.ips[0], espData->wifi.ips[1], espData->wifi.ips[2], 255), 9999);
            // espUdp->sendUDP(testdata, sizeof(testdata));
            delay(3);
            // Current Message
            uint8_t currentData[14];
            currentData[0] = 0x80;
            currentData[1] = 0x81;
            currentData[2] = espData->wifi.ips[3];
            currentData[3] = 250;
            currentData[4] = 8;
            currentData[5] = static_cast<uint8_t>((espData->steer.steerCurrent * 255) / (65535/4));
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
                pid.setSetpoint(100);
                // Serial.println("Setting Setpoint to 100");
            }
            // pid.setSetpoint(100);
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

    if (millis() - espData->steer.watchdog > 2000){
        espData->steer.status = 0;
    }
    
    if (espData->steer.status == 1){
        pid.setSetpoint(espData->steer.targetSteerAngle);
        pid.update(espData->steer.actSteerAngle);
        espData->steer.pidCmd = pid.getOutput();
        motorDriver.setOutput(espData->steer.pidCmd);
    } else {
        pid.setSetpoint(0);
        pid.update(0);
        espData->steer.pidCmd = pid.getOutput();
        motorDriver.setOutput(espData->steer.pidCmd);
    }
}


void ESPsteer::taskHandler(void *param) {
    ESPsteer* instance = (ESPsteer*)param;
    instance->continuousLoop();
}

void ESPsteer::setPIDgains(){
    pid.setManualGains(float(espData->steer.gainP)/200.0, 0, 0);
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
    // pid.enableOutputFilter(espData->steerCfg.pidOutputFilt);
    Serial.println("\tSetting PID Gains");
    setPIDgains();
    pid.setSetpoint(0); // Target setpoint
    // pid.enableInputFilter(espData->steerCfg.pidInputFilt); // Optional input filtering
    // pid.enableAntiWindup(true, 0.8); // Enable anti-windup with 80% threshold
    // pid.setOscillationMode(OscillationMode::Normal); // Set oscillation mode to Normal
    // pid.setOperationalMode(OperationalMode::Tune); // Set operational mode to Tune
    // ********End PID Setup**********

    Serial.println("\tWAS Initialized");
    xTaskCreatePinnedToCore(taskHandler, "taskHandler", 10000, this, 1, NULL, 0);
}

uint32_t ESPsteer::getCurrent() {
    // TODO: Migrate to use i2cManager.getRawReading(2)
    
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
