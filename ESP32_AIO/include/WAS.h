/**
 * @file WAS.h
 * @brief Wheel Angle Sensor (WAS) management for steering position feedback
 * 
 * @details This header defines the WAS class which provides precise wheel angle
 *          measurement and calibration for the steering control system including:
 *          - Analog wheel angle sensor reading via ADS1115 16-bit ADC
 *          - Wireless wheel angle sensor support for retrofit applications
 *          - Sensor calibration and zero-point setting functionality
 *          - Real-time angle measurement with filtering and noise reduction
 *          - Debug mode with ramp testing for calibration and validation
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see WAS.cpp for implementation details
 * @see ESPsteer.h for steering system integration
 */

#ifndef WAS_H
#define WAS_H

// #define WAS_DEBUG

#include "Arduino.h"
#include "ESPdata.h"
#include <Wire.h>
#include "Adafruit_ADS1X15.h"

/**
 * @brief Wheel Angle Sensor management class
 * 
 * @details Provides comprehensive wheel angle sensing functionality including
 *          sensor initialization, continuous measurement, calibration procedures,
 *          and support for both wired (ADC) and wireless sensor configurations.
 */
class WAS{
    public:
        /**
         * @brief Constructor for wheel angle sensor management
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         * @param ads Pointer to ADS1115 ADC for analog sensor readings
         * 
         * @details Initializes WAS system with data management and ADC interfaces
         *          for precise angle measurement and calibration.
         */
        WAS(ESPdata* vars, Adafruit_ADS1115* ads);
        
        /**
         * @brief Initializes wheel angle sensor hardware
         * 
         * @details Configures ADC settings, sensor calibration parameters,
         *          and establishes baseline measurements for angle calculation.
         */
        void init();
        
        /**
         * @brief Main wheel angle sensor processing loop
         * 
         * @details Performs continuous wheel angle measurement including:
         *          - ADC reading and conversion to angle
         *          - Sensor filtering and noise reduction
         *          - Wireless sensor data processing (if enabled)
         *          - Angle calculation and validation
         * 
         * @note Should be called regularly for real-time angle feedback
         */
        void loop();
        
        /**
         * @brief Calibrates the steering zero position
         * 
         * @details Sets the current wheel position as the zero reference point
         *          for steering angle calculations. Used during initial setup
         *          and periodic calibration procedures.
         */
        void zeroSteerAngle();

    private:
        #ifdef WAS_DEBUG
        float rampValue;            ///< @brief Debug ramp test value
        float rampIncrement;        ///< @brief Debug ramp increment (40 units over 10 seconds, with 10ms delay)
        float rampDirection;        ///< @brief Debug ramp direction (1 for up, -1 for down)
        uint32_t lastUpdateTime;    ///< @brief Debug last update timestamp
        #endif
        
        /**
         * @brief Updates debug ramp value for testing
         * 
         * @details Generates a test ramp signal for sensor validation and
         *          calibration procedures when WAS_DEBUG is enabled.
         */
        void updateRampValue();
        
        ESPdata* espData;           ///< @brief Pointer to central data management system
        Adafruit_ADS1115* ads;      ///< @brief Pointer to ADS1115 ADC for sensor readings
       
        
};

#endif