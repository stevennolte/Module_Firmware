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

MotorDriver::MotorDriver(ESPconfig* vars, Adafruit_MCP23X17* mcp){
    espConfig = vars;
    this->mcp = mcp;
    
    inaPin = espConfig->gpioDefs.MOTOR_A_PIN;
    inbPin = espConfig->gpioDefs.MOTOR_B_PIN;
    pwmPin = espConfig->gpioDefs.MOTOR_PWM_PIN;
    enaPin = espConfig->gpioDefs.ENA;
    enbPin = espConfig->gpioDefs.ENB;
    
    
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
        .gpio_num = espConfig->gpioDefs.MOTOR_PWM_PIN,
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
    
    if (espConfig->progData.mcpState == 1){
        mcp->pinMode(enaPin, OUTPUT);  //MCP23017 has the diagnostics pins as inputs
        mcp->pinMode(enbPin, OUTPUT); 
        mcp->digitalWrite(enaPin, LOW);
        mcp->digitalWrite(enbPin, LOW); 
    }
    

    //Set outputs to low
    digitalWrite(inaPin, LOW);
    digitalWrite(inbPin, LOW);
    digitalWrite(pwmPin, LOW);
    
}



void MotorDriver::setOutput(float value){
    // Serial.println(value);
    float minScalar = float(espConfig->steerCfg.minPWM)/255.0;
    if (value > 0.01){
        
        value = max(value, minScalar);
        dirCmd = 1;
        enable();
        setCW();
    } else if (value < -0.01){
        
        value = max(abs(value), minScalar)*-1.0;
        dirCmd = 2;
        enable();
        setCCW();
    } else {
        disable();
        
        dirCmd = 0;
    }
    
    float scalar = maxPWM * (float(espConfig->steerCfg.highPWM)/255.0);


    cmdValue = uint16_t((float(maxPWM)) * abs(value));
    espConfig->steerData.pwmCmd = min(float(cmdValue),scalar);
    // Serial.println(cmdValue);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, cmdValue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void MotorDriver::enable(){
    mcp->digitalWrite(enaPin, HIGH);
    mcp->digitalWrite(enbPin, HIGH);
}

void MotorDriver::disable(){
    mcp->digitalWrite(enaPin, LOW);
    mcp->digitalWrite(enbPin, LOW);
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