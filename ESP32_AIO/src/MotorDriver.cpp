/**
 * @file MotorDriver.cpp
 * @brief Implementation of motor driver control for steering system actuator
 * 
 * @details This file implements the MotorDriver class functionality for
 *          controlling the steering motor including PWM speed control,
 *          direction switching, and safety management using ESP32 LEDC
 *          peripheral and MCP23017 I/O expander.
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see MotorDriver.h for class interface definition
 * @see MCPManager.h for I/O control integration
 */

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

/**
 * @brief Constructor for motor driver control system
 * 
 * @param vars Pointer to ESPdata singleton for configuration access
 * 
 * @details Initializes motor driver with pin assignments from configuration
 *          and establishes connection to MCP23017 I/O expander for control signals.
 */
MotorDriver::MotorDriver(ESPdata* vars) : i2cManager(I2CManager::getInstance()) {
    espData = vars;
    
    inaPin = espData->pins.MOTOR_A_PIN;
    inbPin = espData->pins.MOTOR_B_PIN;
    pwmPin = espData->pins.MOTOR_PWM_PIN;
    enaPin = espData->pins.ENA;
    enbPin = espData->pins.ENB;
}

/**
 * @brief Initialize motor driver hardware and PWM system
 * 
 * @details Configures ESP32 LEDC peripheral for PWM generation and
 *          sets up MCP23017 pins for motor direction and enable control.
 *          Initializes motor in disabled state for safety.
 */
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
    
    

    //Set outputs to low
    digitalWrite(inaPin, LOW);
    digitalWrite(inbPin, LOW);
    digitalWrite(pwmPin, LOW);
    return;
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
    i2cManager.enableMotor();
}

void MotorDriver::disable(){
    i2cManager.disableMotor();
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
