#include "ESPsteer.h"

// #define PRINT_PID_DEBUG

//TODO: SET STEER ANGLE, IMU DATA, ETC DURING TIMEOUT

ESPsteer::ESPsteer(ESPdata* vars, Adafruit_ADS1115* ads, Adafruit_MCP23X17* mcp) : motorDriver(vars, mcp), was(vars, ads), pid(-1.0,1.0, TuningMethod::Manual) {
    espData = vars;
    this->ads = ads;
    this->mcp = mcp;
    
}

void ESPsteer::continuousLoop() {
    while (true) {
        vTaskDelay(3);
        if (espData->steerCfg.settingsUpdated = 1){
            setPIDgains();
            if (espData->steerCfg.set0 == 1){
                // TODO: Set steer angle offset
                
                espData->steerCfg.set0 = 0;
            }
            espData->steerCfg.settingsUpdated = 0;
        }
        was.loop();
        espData->steerData.steerCurrent = getCurrent();
        espData->steerData.testState = getTestState();
        // Serial.println(espData->steerData.testState);
        // vTaskDelay(1000);
        if (millis() - espData->steerData.lastSteerOutMsgTime > espData->steerCfg.steerMsgRate) {

            espData->steerData.lastSteerOutMsgTime = millis();
            uint8_t testdata[14];
            testdata[0] = 0x80;
            testdata[1] = 0x81;
            testdata[2] = espData->wifiCfg.ips[3];
            testdata[3] = 253;
            testdata[4] = 8;
            testdata[5] = static_cast<uint16_t>(espData->steerData.actSteerAngle*100) & 0xFF;
            testdata[6] = static_cast<uint16_t>(espData->steerData.actSteerAngle*100) >> 8;
            testdata[7] = 9999 & 0xFF;
            testdata[8] = 9999 >> 8;
            testdata[9] = 8888 & 0xFF;
            testdata[10] = 8888 >> 8;
            if (espData->joystickData.switchStates[6] == 1) {
                testdata[11] |= 0x01; // Set the first bit to 1
            } else {
                testdata[11] &= ~0x01; // Clear the first bit to 0
            }
                        
            // Set the second bit of testdata[11] to switchStates[7]
            if (espData->joystickData.switchStates[7] == 1) {
                testdata[11] |= 0x02; // Set the second bit to 1
            } else {
                testdata[11] &= ~0x02; // Clear the second bit to 0
            }
            // testdata[11] = 0;
            testdata[12] = static_cast<uint8_t>((espData->steerData.pwmCmd * 255) / 8109);
            testdata[13] = espUdp->calcChecksum(testdata, sizeof(testdata));
            espUdp->udp.writeTo(testdata, sizeof(testdata), IPAddress(espData->wifiCfg.ips[0], espData->wifiCfg.ips[1], espData->wifiCfg.ips[2], 255), 9999);
            // espUdp->sendUDP(testdata, sizeof(testdata));
            delay(3);
            // Current Message
            uint8_t currentData[14];
            currentData[0] = 0x80;
            currentData[1] = 0x81;
            currentData[2] = espData->wifiCfg.ips[3];
            currentData[3] = 250;
            currentData[4] = 8;
            currentData[5] = static_cast<uint8_t>((espData->steerData.steerCurrent * 255) / (65535/4));
            currentData[13] = espUdp->calcChecksum(currentData, sizeof(currentData));
            espUdp->udp.writeTo(currentData, sizeof(currentData), IPAddress(espData->wifiCfg.ips[0], espData->wifiCfg.ips[1], espData->wifiCfg.ips[2], 255), 9999);
        }
        
        
        switch (espData->steerData.status){
            case 1:
                steerLoop();
                break;
            case 0:
                // if (espData->steerData.testState != 0){
                steerTestLoop();
                // }
                break;
        }
        
    }
}

void ESPsteer::steerTestLoop(){
    // Serial.println("Steer Test Loop");
    switch (espData->steerData.testState){
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
    Serial.print(espData->steerData.testState);
    Serial.print("\tSetpoint: ");
    Serial.print(pid.getSetpoint());
    Serial.print("\tOutput: ");
    Serial.println(pid.getOutput());
    #endif
}

void ESPsteer::steerLoop(){
    
    if (millis() - espData->steerData.watchdog > 2000){
        espData->steerData.status = 0;
    }
    
    if (espData->steerData.status == 1){
        pid.setSetpoint(espData->steerData.targetSteerAngle);
        pid.update(espData->steerData.actSteerAngle);
        espData->steerData.pidCmd = pid.getOutput();
        motorDriver.setOutput(espData->steerData.pidCmd);
    } else {
        pid.setSetpoint(0);
        pid.update(0);
        espData->steerData.pidCmd = pid.getOutput();
        motorDriver.setOutput(espData->steerData.pidCmd);
    }
}


void ESPsteer::taskHandler(void *param) {
    ESPsteer* instance = (ESPsteer*)param;
    instance->continuousLoop();
}

void ESPsteer::setPIDgains(){
    pid.setManualGains(float(espData->steerCfg.gainP)/200.0, 0, 0);
}

void ESPsteer::begin(ESPudp* espUdp) {
    this->espUdp = espUdp;
    pinMode(espData->gpioDefs.STEER_TEST_PIN, INPUT);
    _status = espData->steerData.status;
    espData->joystickData.testInfo.testa

    motorDriver.init();
    Serial.println("\tMotor Driver Initialized");
    was.init();

    // *********Start PID Setup**********
    // pid.enableOutputFilter(espData->steerCfg.pidOutputFilt);
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
    return ads->readADC_SingleEnded(2);
}

uint8_t ESPsteer::getTestState(){
    uint32_t reading = analogReadMilliVolts(espData->gpioDefs.STEER_TEST_PIN);
    if (reading < 700){
        return 1;
    } else if (reading > 3000){
        return 2;
    } 
    return 0;
}
