/**
 * @file MainPower.h
 * @brief Main power control and monitoring system for ESP32-AIO controller
 * 
 * @details This header defines the MainPower class which provides comprehensive
 *          power management functionality including:
 *          - System power control and shutdown management
 *          - Current monitoring for load analysis and protection
 *          - Power state management for energy efficiency
 *          - Multi-threaded power monitoring for real-time feedback
 *          - Integration with MCP23017 for power control signals
 *          - ADC-based current sensing for load monitoring
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see MainPower.cpp for implementation details
 * @see MCPManager.h for I/O control integration
 */

#ifndef MAINPOWER_H
#define MAINPOWER_H
#include "Arduino.h"
#include "ESPdata.h"
#include "I2C_Manager.h"

/**
 * @brief Main power control and monitoring class
 * 
 * @details Manages system power control including shutdown procedures,
 *          current monitoring for load analysis, and power state management.
 *          Operates in a dedicated FreeRTOS task for continuous monitoring.
 */
class MainPower
{
    public:
        /**
         * @brief Constructor for main power management system
         * 
         * @param config Pointer to ESPdata singleton for configuration access
         * 
         * @details Initializes power management with configuration for comprehensive power control and monitoring.
         */
        MainPower(ESPdata* config);
        
        /**
         * @brief Start the power monitoring task
         * 
         * @details Creates and starts a FreeRTOS task for continuous power
         *          monitoring and control. Task runs at configurable priority
         *          for real-time power management.
         */
        void startTask();
        
    private:
        /// @brief Power enable state flag
        uint8_t _powerOn;
        /// @brief Main power control pin number
        uint8_t _mainPowerPin;
        
        /**
         * @brief Read current consumption from ADC
         * 
         * @details Measures system current consumption using the ADS1115 ADC
         *          and updates the power monitoring data structure.
         */
        void getCurrent();
        
        /**
         * @brief Static task handler for FreeRTOS task creation
         * 
         * @param param Pointer to MainPower instance for task context
         */
        static void taskHandler(void *param);
        
        /**
         * @brief Continuous monitoring loop for power management
         * 
         * @details Main task function that continuously monitors power
         *          consumption and manages power control functions.
         */
        void continuousLoop();
        
        /// @brief Pointer to ESPdata singleton for configuration access
        ESPdata* _data;
        /// @brief Reference to I2CManager singleton for I/O control
        I2CManager& _i2cManager;

};



#endif