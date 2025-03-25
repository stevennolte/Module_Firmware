#include "ESPsteer.h"

// #define PRINT_PID_DEBUG

//TODO: SET STEER ANGLE, IMU DATA, ETC DURING TIMEOUT

ESPsteer::ESPsteer(ESPconfig* vars, Adafruit_ADS1115* ads, Adafruit_MCP23X17* mcp) : motorDriver(vars, mcp), was(vars, ads), pid(-1.0,1.0, TuningMethod::Manual) {
    espConfig = vars;
    this->ads = ads;
    this->mcp = mcp;
    
}

void ESPsteer::continuousLoop() {
    while (true) {
        vTaskDelay(10);
        if (espConfig->steerCfg.settingsUpdated = 1){
            setPIDgains();
            espConfig->steerCfg.settingsUpdated = 0;
        }
        was.loop();
        espConfig->steerData.steerCurrent = getCurrent();
        espConfig->steerData.testState = getTestState();
        // Serial.println(espConfig->steerData.testState);
        // vTaskDelay(1000);
        if (millis() - espConfig->steerData.lastSteerOutMsgTime > espConfig->steerCfg.steerMsgRate) {

            espConfig->steerData.lastSteerOutMsgTime = millis();
            uint8_t testdata[14];
            testdata[0] = 0x80;
            testdata[1] = 0x81;
            testdata[2] = espConfig->wifiCfg.ips[3];
            testdata[3] = 253;
            testdata[4] = 8;
            testdata[5] = static_cast<uint16_t>(espConfig->steerData.actSteerAngle*100) & 0xFF;
            testdata[6] = static_cast<uint16_t>(espConfig->steerData.actSteerAngle*100) >> 8;
            testdata[7] = 9999 & 0xFF;
            testdata[8] = 9999 >> 8;
            testdata[9] = 8888 & 0xFF;
            testdata[10] = 8888 >> 8;
            testdata[11] = 101;
            testdata[12] = 100;
            testdata[13] = espUdp->calcChecksum(testdata, sizeof(testdata));
            espUdp->udp.writeTo(testdata, sizeof(testdata), IPAddress(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1], espConfig->wifiCfg.ips[2], 255), 9999);
            // espUdp->sendUDP(testdata, sizeof(testdata));
        }
        
        
        switch (espConfig->steerData.status){
            case 1:
                steerLoop();
                break;
            case 0:
                // if (espConfig->steerData.testState != 0){
                steerTestLoop();
                // }
                break;
        }
        
    }
}

void ESPsteer::steerTestLoop(){
    // Serial.println("Steer Test Loop");
    switch (espConfig->steerData.testState){
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
    Serial.print(espConfig->steerData.testState);
    Serial.print("\tSetpoint: ");
    Serial.print(pid.getSetpoint());
    Serial.print("\tOutput: ");
    Serial.println(pid.getOutput());
    #endif
}

void ESPsteer::steerLoop(){
    if (millis() - espConfig->steerData.watchdog > 2000){
        espConfig->steerData.status = 0;
    }
    if (espConfig->steerData.status == 1){
        pid.setSetpoint(espConfig->steerData.targetSteerAngle);
        pid.update(espConfig->steerData.actSteerAngle);
        espConfig->steerData.pidCmd = pid.getOutput();
        motorDriver.setOutput(espConfig->steerData.pidCmd);
    } else {
        pid.setSetpoint(0);
        pid.update(0);
        espConfig->steerData.pidCmd = pid.getOutput();
        motorDriver.setOutput(espConfig->steerData.pidCmd);
    }
}


void ESPsteer::taskHandler(void *param) {
    ESPsteer* instance = (ESPsteer*)param;
    instance->continuousLoop();
}

void ESPsteer::setPIDgains(){
    pid.setManualGains(float(espConfig->steerCfg.gainP)/100.0, 0, 0);
}

void ESPsteer::begin(ESPudp* espUdp) {
    this->espUdp = espUdp;
    pinMode(espConfig->gpioDefs.STEER_TEST_PIN, INPUT);
    _status = espConfig->steerData.status;


    motorDriver.init();
    Serial.println("\tMotor Driver Initialized");
    was.init();

    // *********Start PID Setup**********
    pid.enableOutputFilter(espConfig->steerCfg.pidOutputFilt);
    setPIDgains();
    pid.setSetpoint(0); // Target setpoint
    pid.enableInputFilter(espConfig->steerCfg.pidInputFilt); // Optional input filtering
    pid.enableAntiWindup(true, 0.8); // Enable anti-windup with 80% threshold
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
    uint32_t reading = analogReadMilliVolts(espConfig->gpioDefs.STEER_TEST_PIN);
    if (reading < 700){
        return 1;
    } else if (reading > 3000){
        return 2;
    } 
    return 0;
}