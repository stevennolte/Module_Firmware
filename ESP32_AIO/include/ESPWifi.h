/**
 * @file ESPWifi.h
 * @brief WiFi network management and connectivity for ESP32-AIO system
 * 
 * @details This header defines the ESPWifi class which provides comprehensive
 *          wireless network connectivity management including:
 *          - Multi-SSID WiFi connection with automatic failover
 *          - Access Point mode for configuration and recovery
 *          - Network monitoring and automatic reconnection
 *          - mDNS service discovery and registration
 *          - Multi-threaded network monitoring for reliability
 *          - Dynamic network scanning and selection
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPWifi.cpp for implementation details
 * @see ESPdata.h for network configuration
 */

#ifndef ESPWIFI_H
#define ESPWIFI_H

#include <Arduino.h>
#include "ESPdata.h"
#include "ESPmDNS.h"

/**
 * @brief WiFi network management class for connectivity and monitoring
 * 
 * @details Provides comprehensive WiFi functionality including connection management,
 *          access point creation, network monitoring, and automatic recovery.
 *          Operates in both station and access point modes for maximum flexibility.
 */
class ESPWifi
{
    public:
        /**
         * @brief Attempts to connect to configured WiFi networks
         * 
         * @return uint8_t Connection status (0=failed, 1=connected, 2=timeout)
         * 
         * @details Tries to connect to each configured SSID in sequence until
         *          successful connection is established. Includes timeout handling
         *          and connection quality assessment.
         */
        uint8_t connect();
        
        /**
         * @brief Creates WiFi Access Point for configuration access
         * 
         * @return uint8_t AP creation status (0=failed, 1=success)
         * 
         * @details Sets up WiFi access point mode for emergency configuration
         *          access when normal WiFi connection fails. Uses device name
         *          as SSID with security configuration.
         */
        uint8_t makeAP();
        
        /**
         * @brief Starts WiFi monitoring task for connection reliability
         * 
         * @details Launches background task for continuous WiFi monitoring
         *          including connection health checks and automatic reconnection
         *          when connection is lost.
         */
        void startMonitor();
        
        /**
         * @brief Scans for available WiFi networks
         * 
         * @details Performs active scan for nearby WiFi networks and logs
         *          results for network selection and troubleshooting purposes.
         */
        void scanNetworks();
        
        /**
         * @brief Constructor for WiFi management system
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         * 
         * @details Initializes WiFi system with configuration parameters
         *          including SSID list, passwords, and connection preferences.
         */
        ESPWifi(ESPdata* vars);

    private:
        /**
         * @brief FreeRTOS task handler for WiFi monitoring
         * 
         * @param param Task parameters (pointer to ESPWifi instance)
         * 
         * @details Static task handler function for FreeRTOS multithreading
         */
        static void taskHandler(void *param);
        
        /**
         * @brief Continuous WiFi monitoring loop
         * 
         * @details Main monitoring function that runs continuously in
         *          dedicated FreeRTOS task for connection reliability
         */
        void continuousLoop();
        
        ESPdata* espData;           ///< @brief Pointer to central data management system
};

#endif