/**
 * @file ESPsteer.h
 * @brief Precision steering control system for agricultural auto-guidance
 * 
 * @details This header defines the ESPsteer class which provides comprehensive
 *          steering system control for precision agriculture applications including:
 *          - PID-based steering angle control with auto-tuning capabilities
 *          - Wheel angle sensor (WAS) management for position feedback
 *          - Motor driver control with current monitoring and protection
 *          - Multi-threaded operation for real-time steering response
 *          - Safety systems and emergency override functionality
 *          - Configurable steering parameters and calibration
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPsteer.cpp for implementation details
 * @see MotorDriver.h for motor control interface
 * @see WAS.h for wheel angle sensor functionality
 */

#ifndef ESPSTEER_H
#define ESPSTEER_H

#include "ESPdata.h"
#include "MotorDriver.h"
#include "WAS.h"
#include <Wire.h>
#include "ESPudp.h"
#include "AutoTunePID.h"
#include "I2C_Manager.h"

class ESPudp; ///< @brief Forward declaration for UDP communication class

/**
 * @brief Precision steering control system class
 * 
 * @details Manages the complete steering control system including PID control,
 *          sensor feedback, motor control, and safety monitoring. Operates in
 *          a dedicated FreeRTOS task for real-time performance and provides
 *          comprehensive steering functionality for agricultural guidance systems.
 */
class ESPsteer{
    public:
        /**
         * @brief Initializes the steering control system
         * 
         * @param espUdp Pointer to UDP communication system for external control
         * 
         * @details Sets up steering control task, initializes PID controller,
         *          configures motor driver, and starts the steering control loop
         */
        void begin(ESPudp* espUdp);
        
        /**
         * @brief Reads motor current consumption
         * 
         * @return uint32_t Current consumption in milliamps
         * 
         * @details Measures motor current through ADS1115 ADC for load monitoring
         *          and motor protection functionality
         */
        uint32_t getCurrent();
        
        /**
         * @brief Updates PID controller gain parameters
         * 
         * @details Applies current PID gain settings from configuration to the
         *          active PID controller for steering performance optimization
         */
        void setPIDgains();
        
        /**
         * @brief Constructor for steering control system
         * 
         * @param vars Pointer to ESPdata singleton for configuration and data storage
         *
         * @details Initializes steering control system with data management and
         *          analog sensor interfaces
         */
        ESPsteer(ESPdata* vars);
        
        WAS was; ///< @brief Wheel angle sensor object for position feedback
        
    private:
        /**
         * @brief Steering system test mode processing loop
         * 
         * @details Handles manual steering control and calibration procedures
         *          for system testing and validation
         */
        void steerTestLoop();
        
        /**
         * @brief Main steering control processing loop
         * 
         * @details Implements PID-based steering control including:
         *          - Target angle processing from guidance system
         *          - Wheel angle sensor feedback processing
         *          - PID control calculation and motor command generation
         *          - Safety monitoring and emergency override
         */
        void steerLoop();
        
        uint8_t _status;            ///< @brief Internal steering system status
        
        /**
         * @brief FreeRTOS task handler for steering control
         * 
         * @param param Task parameters (pointer to ESPsteer instance)
         * 
         * @details Static task handler function for FreeRTOS multithreading
         */
        static void taskHandler(void *param);
        
        /**
         * @brief Continuous steering control loop
         * 
         * @details Main steering control function that runs continuously in
         *          dedicated FreeRTOS task for real-time operation
         */
        void continuousLoop();
        
        /**
         * @brief Reads current steering test state
         * 
         * @return uint8_t Test state value from hardware input
         * 
         * @details Determines if system is in test mode for manual operation
         */
        uint8_t getTestState();
        
        ESPdata* espData;           ///< @brief Pointer to central data management system
        I2CManager& i2cManager;  ///< @brief I2C manager for ADC and GPIO expander
        ESPudp* espUdp;
        MotorDriver motorDriver;
        
        AutoTunePID pid;
        
        bool currentLimitLatched;   ///< @brief Persistent overcurrent fault latch state
        float filteredCurrent = 0.0f; ///< @brief Exponential moving average filtered motor current

        // Stiction boost runtime state
        uint32_t _stictionStartTime = 0;  ///< @brief Timestamp when stall was first detected
        float _stictionLastAngle = 0.0f;  ///< @brief Actual angle at stall detection start
        bool _stictionActive = false;     ///< @brief True while stiction boost is being applied

};

#endif