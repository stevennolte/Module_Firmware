/**
 * @file ESPudp.h
 * @brief UDP communication management for agricultural guidance systems
 * 
 * @details This header defines the ESPudp class which provides comprehensive
 *          UDP communication services for precision agriculture applications including:
 *          - AgOpen GPS protocol implementation for guidance data exchange
 *          - NTRIP client functionality for RTK correction data
 *          - Wireless angle sensor (WAS) data reception
 *          - Joystick control data handling for manual override
 *          - Multi-port UDP service management for various data streams
 *          - Packet validation and checksum verification
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPudp.cpp for implementation details
 * @see GPS.h for NMEA data integration
 * @see ESPWifi.h for network connectivity
 */

#ifndef ESPNETWORK_H
#define ESPNETWORK_H

#include "Arduino.h"
#include "AsyncUDP.h"
#include <AsyncTCP.h>

#include "ESPdata.h"
#include "GPS.h"

// Forward declaration of the GPS class
class ESPGPS;

/**
 * @brief UDP communication manager for agricultural guidance systems
 * 
 * @details Manages multiple UDP services for different data streams:
 *          - Port 8888: Main AgOpen GPS communication
 *          - Port 9999: GPS/NMEA data broadcast
 *          - Port 2233: NTRIP correction data reception
 *          - Port 8889: Wireless angle sensor data
 *          - Port 8887: Joystick control data
 * 
 *          Provides packet validation, data parsing, and protocol-specific
 *          handling for each communication channel.
 */
class ESPudp{
    public:
        /// @brief AgOpen GPS reply packet buffer (11 bytes)
        uint8_t aioReply[11];
        
        /**
         * @brief Initialize UDP communication services
         * 
         * @param gps Pointer to GPS manager for NMEA data integration
         * 
         * @details Sets up all UDP listeners on their respective ports and
         *          configures packet handlers for each service type.
         */
        void begin(ESPGPS* gps);
        
        /**
         * @brief Send UDP data packet to AgOpen GPS
         * 
         * @param data Pointer to data buffer to send
         * @param size Size of data buffer in bytes
         * 
         * @details Sends data to the AgOpen GPS application using the
         *          configured IP address and port.
         */
        void sendUDP(uint8_t* data, size_t size);
        
        /**
         * @brief Broadcast GPS/NMEA data via UDP
         * 
         * @param data NMEA sentence string to broadcast
         * 
         * @details Broadcasts GPS data on port 9999 for other systems
         *          to receive positioning information.
         */
        void sendUDPgps(const char * data);
        
        /// @brief Main UDP service for AgOpen GPS communication
        AsyncUDP udp;
        /// @brief GPS data broadcast service (port 9999)
        AsyncUDP udpGPS;
        /// @brief Wireless angle sensor UDP service (port 8889)
        AsyncUDP udpWAS;
        
        /**
         * @brief Calculate packet checksum for data validation
         * 
         * @param data Pointer to data buffer
         * @param size Size of data buffer
         * @return uint8_t Calculated checksum value
         * 
         * @details Computes checksum for packet validation according to
         *          AgOpen GPS protocol specifications.
         */
        uint8_t calcChecksum(uint8_t* data, size_t size);
        
        /**
         * @brief Constructor for UDP communication manager
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         */
        ESPudp(ESPdata* vars);
        
    private:
        /// @brief NTRIP correction data UDP service (port 2233)
        AsyncUDP udpNtrip;
        /// @brief Joystick control UDP service (port 8887)
        AsyncUDP udpJoystick;
        
        ESPdata* espData;
        ESPGPS* _gps;
    
};

#endif