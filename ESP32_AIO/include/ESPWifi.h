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
         * @brief Constructor for WiFi management system
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         */
        ESPWifi(ESPdata* vars);

        /**
         * @brief Starts the WiFi access point (and sets the WiFi mode)
         * 
         * @details Sets up AP / AP+STA / STA-only mode based on wifiMode setting.
         *          Starts the softAP when not in STA-only mode.
         */
        void startAP();

        /**
         * @brief Attempts to connect to a configured STA network
         * 
         * @details Scans for visible networks, picks the first configured match,
         *          and initiates an async WiFi.begin(). Does nothing when
         *          wifiMode == 0 or no networks are configured.
         */
        void connectSTA();

        /**
         * @brief Starts WiFi monitoring task for connection reliability
         * 
         * @details Launches background task for continuous WiFi monitoring
         *          including connection health checks and automatic reconnection
         *          when connection is lost.
         */
        void startMonitor();

        /**
         * @brief Returns the number of stations connected to the AP
         * 
         * @return int Number of connected clients (0 in STA-only mode)
         */
        int getConnectedClients() const;

    private:
        /**
         * @brief FreeRTOS task handler for WiFi monitoring
         * 
         * @param param Task parameters (pointer to ESPWifi instance)
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
        uint32_t staConnectStartTime;   ///< @brief Timestamp when STA connection first attempted
        bool staConnectionTimedOut;     ///< @brief Flag indicating STA connection gave up after timeout
};

#endif
