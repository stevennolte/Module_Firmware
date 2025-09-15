/* NOTES:
    ledc_get_duty(mode, channel)
    ledc_set_duty(mode, channel, cmd)
    ledc_update_duty(mode, channel)
    ledc_get_freq(mode, timer)
    ledc_set_freq(mode, timer, cmd)
*/

#include "Arduino.h"
#include "MotorDriver.h"
#include "driver/ledc.h"

MotorDriver::MotorDriver(ESPdata* vars) : mcpManager(MCPManager::getInstance()) {
    espData = vars;
    
    inaPin = espData->pins.MOTOR_A_PIN;
    inbPin = espData->pins.MOTOR_B_PIN;
    pwmPin = espData->pins.MOTOR_PWM_PIN;
    enaPin = espData->pins.ENA;
    enbPin = espData->pins.ENB;
}



void MotorDriver::init(){
    Serial.println("setting ledc");
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 400,
        .clk_cfg = LEDC_USE_APB_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .gpio_num = espData->pins.MOTOR_PWM_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    Serial.println("Setting PID");
    // Configure PID
   
    


    //Set up the motor driver pins
    pinMode(inaPin, OUTPUT);
    pinMode(inbPin, OUTPUT);
    // pinMode(pwmPin, OUTPUT);
    
    if (espData->program.mcpState == 1 && mcpManager.isInitialized()){
        mcpManager.setupMotorPins();  // Uses ESPdata pin definitions automatically
        Serial.println("MotorDriver: Motor pins configured using MCPManager");
    } else {
        Serial.println("MotorDriver: MCPManager not available, motor enable pins not configured");
    }
    

    //Set outputs to low
    digitalWrite(inaPin, LOW);
    digitalWrite(inbPin, LOW);
    digitalWrite(pwmPin, LOW);
    
}



void MotorDriver::setOutput(float value){
    // Serial.println(value);
    espData->steer.minScalar = float(espData->steer.minPWM)/255.0;
    espData->steer.maxScalar = float(espData->steer.highPWM)/255.0;
    espData->steer.minCmd = maxPWM * espData->steer.minScalar;
    espData->steer.maxCmd = maxPWM * espData->steer.maxScalar;
    if (value > 0.001){
        
        // value = max(value, minScalar);
        dirCmd = 1;
        enable();
        setCW();
    } else if (value < -0.001){
        
        value = abs(value);
        dirCmd = 2;
        enable();
        setCCW();
    } else {
        disable();
        value = 0;
        dirCmd = 0;
    }
    
    
    // Scale value to range [minCMD, maxCMD]
    uint16_t scaledValue = espData->steer.minCmd + value * (espData->steer.maxCmd - espData->steer.minCmd);

    // cmdValue = uint16_t((float(maxPWM)) * abs(value));
    // espData->steerData.pwmCmd = min(float(cmdValue),scalar);
    cmdValue = scaledValue;
    espData->steer.pwmCmd = scaledValue;
    
    // Serial.println(cmdValue);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, cmdValue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void MotorDriver::enable(){
    if (mcpManager.isInitialized()) {
        mcpManager.enableMotor();  // Uses ESPdata pin definitions automatically
    } else {
        Serial.println("MotorDriver: Cannot enable motor - MCPManager not initialized");
    }
}

void MotorDriver::disable(){
    if (mcpManager.isInitialized()) {
        mcpManager.disableMotor();  // Uses ESPdata pin definitions automatically
    } else {
        Serial.println("MotorDriver: Cannot disable motor - MCPManager not initialized");
    }
    digitalWrite(inbPin, LOW);
    digitalWrite(inaPin, LOW);
}

void MotorDriver::setCW(){
    digitalWrite(inbPin, LOW);
    digitalWrite(inaPin, HIGH);
}

void MotorDriver::setCCW(){
    digitalWrite(inaPin, LOW);
    digitalWrite(inbPin, HIGH);
}
