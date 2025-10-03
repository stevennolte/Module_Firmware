/**
 * @file GPS.h
 * @brief GPS receiver and IMU sensor management for precision agriculture navigation
 * 
 * @details This header defines the ESPGPS class which manages GPS receiver communication,
 *          NMEA sentence parsing, IMU sensor integration, and NTRIP correction data handling.
 *          Provides comprehensive navigation and positioning capabilities including:
 *          - Multi-constellation GNSS receiver support (GPS, GLONASS, Galileo, BeiDou)
 *          - Real-time kinematic (RTK) precision positioning
 *          - Inertial measurement unit (IMU) integration for heading and attitude
 *          - NTRIP client for differential correction data
 *          - Position quality monitoring and status indication
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see GPS.cpp for implementation details
 * @see ESPudp.h for NTRIP client functionality
 */

#ifndef GPS_H
#define GPS_H

#include "Arduino.h"
#include "ESPdata.h"
#include "ESPudp.h"
#include "ESPdata_macros.h"
#include <zNMEAParser.h>
#include <SparkFun_Unicore_GNSS_Arduino_Library.h>
#include "Adafruit_BNO08x_RVC.h"
#include "Adafruit_MCP23X17.h"
#include "MCPManager.h"

class ESPudp; ///< @brief Forward declaration for UDP communication class

/**
 * @brief GPS receiver and IMU sensor management class
 * 
 * @details Provides complete navigation system functionality including:
 *          - GPS receiver initialization and communication
 *          - NMEA sentence parsing and data extraction
 *          - IMU sensor integration for attitude determination
 *          - Position quality assessment and monitoring
 *          - Status LED control for fix quality indication
 */
class ESPGPS{
    public:
        /**
         * @brief Constructor for GPS and IMU management system
         * 
         * @param vars Pointer to ESPdata singleton for configuration and data storage
         * @param gpsSerial Pointer to hardware serial port for GPS communication
         * @param bnoSerial Pointer to hardware serial port for BNO055 IMU communication
         * 
         * @details Initializes GPS and IMU communication interfaces using the provided
         *          serial ports and links to the central data management system
         */
        ESPGPS(ESPdata* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial);

        /**
         * @brief Initializes GPS system with UDP communication support
         * 
         * @param espUdp Pointer to UDP client for NTRIP correction data
         * 
         * @details Sets up GPS receiver communication, configures NMEA message rates,
         *          initializes IMU sensor, and establishes NTRIP client connection
         */
        void init(ESPudp* espUdp);
        
        /**
         * @brief Alternative initialization method using MCPManager singleton
         * 
         * @param espUdp Pointer to UDP client for NTRIP correction data
         * 
         * @details Provides singleton-based initialization for cleaner architecture
         *          and improved resource management
         */
        void initWithSingleton(ESPudp* espUdp);
        
        /**
         * @brief Main GPS processing loop for continuous operation
         * 
         * @details Handles continuous GPS data processing including:
         *          - NMEA sentence reception and parsing
         *          - IMU data acquisition and processing
         *          - Position quality assessment
         *          - Status indicator updates
         *          - Data structure population
         * 
         * @note Should be called regularly in the main program loop
         */
        void continuousLoop();
        static void taskHandler(void *param);
        void buildNmea();
        void calculateChecksum();
        void test();
        void sendNTRIP(uint8_t* data, uint8_t len);
        // static void errorHandler();
        void GGA_Handler();
        void displayInfo();
        void imuHandler();
        
        // MCPManager helper methods
        void updateGPSIndicators();
        void setGPSIndicators(bool hasGPSFix, bool hasRTKFix);
    private:
        static ESPGPS* instance;
        static void staticGGA_Handler();
        char fixTime[12];
        uint32_t imuWatchdog;
        uint32_t gpsWatchdog;
        uint8_t _gpsFixIndPin;
        uint8_t _rtkFixIndPin;
        ESPdata* espData;
        ESPudp* espUdp;
        MCPManager& mcpManager;  // Reference to MCPManager singleton
        NMEAParser<2> parser;
        UM980 myGNSS;
        HardwareSerial* gpsSerial;
        HardwareSerial* bnoSerial;

        Adafruit_MCP23X17 mcp;
        Adafruit_BNO08x_RVC rvc;
};

#endif