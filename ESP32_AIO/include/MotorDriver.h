/**
 * @file MotorDriver.h
 * @brief Motor driver control interface for steering system actuator
 * 
 * @details This header defines the MotorDriver class which provides comprehensive
 *          control of the steering motor driver including:
 *          - Bidirectional motor control with direction switching
 *          - PWM speed control with configurable limits
 *          - Motor enable/disable functionality for safety
 *          - Current monitoring and protection
 *          - Integration with MCP23017 I/O expander for control signals
 * 
 * @author Steve Gavel  
 * @date 2024
 * @version 4.6.1
 * 
 * @see MotorDriver.cpp for implementation details
 * @see ESPsteer.h for steering system integration
 */

#ifndef MOTORDRIVER_H
#define MOTORDRIVER_H

#include "Arduino.h"
#include "ESPdata.h"
#include <Adafruit_MCP23X17.h>
#include "MCPManager.h"

/**
 * @brief Motor driver control class for steering actuator
 * 
 * @details Provides complete motor control functionality including direction control,
 *          speed regulation via PWM, and safety features. Interfaces with both
 *          ESP32 native pins and MCP23017 I/O expander for comprehensive control.
 */
class MotorDriver{
    public:
        uint8_t state;              ///< @brief Current motor driver state
        
        /**
         * @brief Initializes motor driver hardware interface
         * 
         * @details Configures GPIO pins for motor control including:
         *          - PWM output for speed control
         *          - Direction control pins (INA/INB)
         *          - Enable pins for motor driver activation
         */
        void init();
        
        /**
         * @brief Sets motor output value with direction and speed
         * 
         * @param value Motor command (-1.0 to +1.0, negative=CCW, positive=CW)
         * 
         * @details Converts normalized motor command to appropriate PWM and direction
         *          signals. Handles direction switching and speed scaling.
         */
        void setOutput(float value);
        
        /**
         * @brief Enables the motor driver
         * 
         * @details Activates motor driver enable pins to allow motor operation.
         *          Must be called before motor commands will take effect.
         */
        void enable();
        
        /**
         * @brief Disables the motor driver for safety
         * 
         * @details Deactivates motor driver enable pins to prevent motor operation.
         *          Used for emergency stops and safety interlocks.
         */
        void disable();
        
        /**
         * @brief Sets motor direction to clockwise
         * 
         * @details Configures direction control pins for clockwise rotation.
         *          Used internally by setOutput() function.
         */
        void setCW();
        
        /**
         * @brief Sets motor direction to counter-clockwise
         * 
         * @details Configures direction control pins for counter-clockwise rotation.
         *          Used internally by setOutput() function.
         */
        void setCCW();
        
        /**
         * @brief Constructor for motor driver control
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         * 
         * @details Initializes motor driver with pin assignments from configuration.
         *          Uses MCPManager singleton for I/O expander control.
         */
        MotorDriver(ESPdata* vars);
        
    private:
        uint16_t maxPWM=8000;       ///< @brief Maximum PWM value for speed control
        uint8_t inaPin;             ///< @brief Motor direction control pin A
        uint8_t inbPin;             ///< @brief Motor direction control pin B
        uint8_t pwmPin;             ///< @brief PWM output pin for speed control
        uint8_t enaPin;             ///< @brief Motor driver enable pin A
        uint8_t enbPin;             ///< @brief Motor driver enable pin B
        uint16_t cmdValue;
        
        uint8_t dirCmd;
        float pidInput;
        float pidOutput;

        ESPdata* espData;
        MCPManager& mcpManager;  // Reference to MCPManager singleton
       

};

#endif