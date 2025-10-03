/**
 * @file GPS.cpp
 * @brief Implementation of GPS receiver and IMU sensor management system
 * 
 * @details This file implements the ESPGPS class which provides comprehensive
 *          navigation and positioning functionality for precision agriculture.
 *          Integrates GPS receiver, IMU sensor, and NTRIP correction data handling
 *          for centimeter-level positioning accuracy.
 *          
 *          Key features implemented:
 *          - NMEA sentence parsing for GPS data extraction
 *          - IMU sensor integration for heading and attitude
 *          - Position quality monitoring and status indication
 *          - NTRIP client for differential correction data
 *          - Multi-constellation GNSS support
 *          - Real-time position and navigation data processing
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see GPS.h for class interface definition
 * @see ESPudp.h for NTRIP client functionality
 */

#include "GPS.h"

/// @brief Static singleton instance pointer for GPS management
ESPGPS* ESPGPS::instance = nullptr;

/**
 * @brief Constructor for GPS and IMU management system
 * 
 * @param vars Pointer to ESPdata singleton for configuration and data storage
 * @param gpsSerial Pointer to hardware serial port for GPS communication
 * @param bnoSerial Pointer to hardware serial port for BNO055 IMU communication
 * 
 * @details Initializes GPS receiver and IMU interfaces using singleton pattern.
 *          Sets up serial communication ports and configures status indicator pins.
 *          Links to central data management system for configuration access.
 */
ESPGPS::ESPGPS(ESPdata* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial) 
    : parser(), rvc(), myGNSS(), mcpManager(MCPManager::getInstance()) {
    espData = vars;
    this->gpsSerial = gpsSerial;
    this->bnoSerial = bnoSerial;
    
    _gpsFixIndPin = espData->pins.gpsFix;
    _rtkFixIndPin = espData->pins.rtkFix;
    instance = this;
}

/**
 * @brief NMEA GGA sentence handler for GPS position data
 * 
 * @details Parses NMEA GGA (Global Positioning System Fix Data) sentences to extract:
 *          - Fix time (UTC)
 *          - Latitude and longitude coordinates
 *          - Fix quality indicator
 *          - Number of satellites in use
 *          - Horizontal dilution of precision (HDOP)
 *          - Altitude above mean sea level
 *          - Age of differential GPS corrections
 * 
 *          Updates the GPS data structure with parsed values for use by
 *          navigation and guidance systems.
 * 
 * @note Called automatically by NMEA parser when GGA sentence is received
 * @see VTG_Handler(), parser.h
 */
void ESPGPS::GGA_Handler() //Rec'd GGA
{
    // fix time
    parser.getArg(0, espData->gps.fixTime);

    // latitude
    parser.getArg(1, espData->gps.latitude);
    parser.getArg(2, espData->gps.latNS);

    // longitude
    parser.getArg(3, espData->gps.longitude);
    // Serial.println(longitude);
    parser.getArg(4, espData->gps.lonEW);

    // fix quality
    parser.getArg(5, espData->gps.fixQuality);

    // satellite #
    parser.getArg(6, espData->gps.numSats);

    // HDOP
    parser.getArg(7, espData->gps.HDOP);

    // altitude
    parser.getArg(8, espData->gps.altitude);

    // time of last DGPS update
    parser.getArg(12, espData->gps.ageDGPS);

    // Update GPS indicators based on fix quality using MCPManager member
    if (mcpManager.isInitialized()) {
        // GPS fix indicator: ON if we have any fix (quality > 0)
        bool hasGPSFix = (atoi(espData->gps.fixQuality) > 0);
        mcpManager.setGPSFix(hasGPSFix);  // Uses ESPdata pin definitions
        
        // RTK fix indicator: ON if we have RTK fix (quality 4 or 5)
        int quality = atoi(espData->gps.fixQuality);
        bool hasRTKFix = (quality == 4 || quality == 5);
        mcpManager.setRTKFix(hasRTKFix);  // Uses ESPdata pin definitions
    }
    imuHandler();
    buildNmea();
}
void ESPGPS::staticGGA_Handler(){
  if(instance){
    instance->GGA_Handler();
  }
}
void ESPGPS::init(ESPudp* espUdp){
    this->espUdp = espUdp;
    
    // Setup GPS indicators using MCPManager member
    if (mcpManager.isInitialized()) {
        mcpManager.setupGPSIndicators();  // Uses ESPdata pin definitions
        mcpManager.testGPSIndicators();   // Uses ESPdata pin definitions
        Serial.println("GPS: Indicator pins configured using MCPManager");
    } else {
        Serial.println("GPS: MCPManager not initialized, skipping indicator setup");
    }
   
    parser.addHandler("G-GGA", staticGGA_Handler);
    // parser.setErrorHandler(errorHandler);
    // parser.addHandler("G-GGA", GPS::GGA_Handler);
    // parser.addHandler("G-VTG", VTG_Handler);
    // if (!rvc.begin(&bnoSerial)){
    //     Serial.println("BNO08x not detected");
    // }
    Serial.println("Starting BNO08x");
    if (rvc.begin(bnoSerial)){
        espData->gps.imuState = 1;
    } else {
        espData->gps.imuState = 2;
        Serial.println("RVC Start Failed");
    }
    if (!espData->gps.externalGPS){
        Serial.println("Trying to start UM980");
        uint32_t gpsStart = millis();
        uint8_t gpsTryCnt = 0;
        bool gpsConnected = false;
        
        // Structure to pass parameters to GPS initialization task
        struct GPSInitParams {
            UM980* gnss;
            HardwareSerial* serial;
            volatile bool* completed;
            volatile bool* result;
        };
        
        while (gpsTryCnt < 2 && !gpsConnected){
            Serial.print("GPS connection attempt ");
            Serial.print(gpsTryCnt + 1);
            Serial.print("/2: ");
            
            // Task completion and result flags
            volatile bool taskCompleted = false;
            volatile bool connectionResult = false;
            
            // Parameters for the task
            GPSInitParams params = {
                &myGNSS,
                gpsSerial,
                &taskCompleted,
                &connectionResult
            };
            
            TaskHandle_t gpsInitTask = NULL;
            
            // Create GPS initialization task
            xTaskCreate([](void* param) {
                GPSInitParams* p = (GPSInitParams*)param;
                *p->result = p->gnss->begin(*p->serial);
                *p->completed = true;
                vTaskDelete(NULL);
            }, "GPSInit", 4096, &params, 1, &gpsInitTask);
            
            // Wait for completion or timeout (2 seconds)
            uint32_t attemptStart = millis();
            while (!taskCompleted && (millis() - attemptStart < 2000)) {
                delay(100);
                Serial.print(".");
            }
            
            if (taskCompleted && connectionResult) {
                Serial.println(" Connected!");
                gpsConnected = true;
            } else {
                Serial.println(" Timeout");
                if (gpsInitTask != NULL) {
                    vTaskDelete(gpsInitTask); // Force delete the task if it's still running
                }
                gpsTryCnt++;
            }
        }
        Serial.println();
        if (!gpsConnected) //Give the serial port over to the library
        {
            espData->gps.state = 2;
            Serial.println("UM980 failed to respond. Check ports and baud rates.");
            uint32_t gpsEnd = millis();
            Serial.print("GPS initialization time: ");
            Serial.print(gpsEnd - gpsStart);
            Serial.println(" ms");
        } else {
            espData->gps.state = 1;
            Serial.println("UM980 detected!");
            myGNSS.disableOutput();
            myGNSS.setModeRoverAutomotive();
            myGNSS.setNMEAMessage("GPGGA", .1); 
            myGNSS.setNMEAMessage("GPGSA", .1); 
            myGNSS.setNMEAMessage("GPGST", .1); 
            myGNSS.setNMEAMessage("GPRMC", .1); 
            myGNSS.setNMEAMessage("GPGSV", .1);
            // myGNSS.
            myGNSS.saveConfiguration();
        }
        Serial.print("Time to start gps: ");
        Serial.print(millis() - gpsStart);
        Serial.println(" ms");
        xTaskCreatePinnedToCore(taskHandler, "taskHandler", 10000, this, 1, NULL, 0);
    }
    Serial.println("GPS initialization complete");
    // delay(1000);
    
}


void ESPGPS::calculateChecksum(void)
{
  int16_t sum = 0;
  int16_t inx = 0;
  char tmp;

  // The checksum calc starts after '$' and ends before '*'
  for (inx = 1; inx < 200; inx++)
  {
    tmp = espData->gps.nmea[inx];

    // * Indicates end of data and start of checksum
    if (tmp == '*')
    {
      break;
    }

    sum ^= tmp;    // Build checksum
  }

  byte chk = (sum >> 4);
  char hex[2] = { espData->gps.asciiHex[chk], 0 };
  strcat(espData->gps.nmea, hex);

  chk = (sum % 16);
  char hex2[2] = { espData->gps.asciiHex[chk], 0 };
  strcat(espData->gps.nmea, hex2);
}



void ESPGPS::buildNmea()
{
    strcpy(espData->gps.nmea, "");
    strcat(espData->gps.nmea, "$PANDA,");
    strcat(espData->gps.nmea, espData->gps.fixTime);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.latitude);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.latNS);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.longitude);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.lonEW);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.fixQuality);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.numSats);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.HDOP);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.altitude);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.ageDGPS);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.speedKnots);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.imuHeading);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.imuRoll);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.imuPitch);
    strcat(espData->gps.nmea, ",");
    strcat(espData->gps.nmea, espData->gps.imuYawRate);
    strcat(espData->gps.nmea, "*");

    calculateChecksum();

    strcat(espData->gps.nmea, "\r\n");


    if (espData->wifi.state == 1)   //If ethernet running send the GPS there
    {
        int len = strlen(espData->gps.nmea);
        // udpMethods.udp.writeTo(nmea,len,IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        // TODO: udpMethods.udp.broadcastTo(nmea,9999);
        // espUdp->udpNtrip.broadcastTo(espData->gps.nmea,9999);
        espUdp->sendUDPgps(espData->gps.nmea);
        // Eth_udpPAOGI.beginPacket(Eth_ipDestination, portDestination);
        // Eth_udpPAOGI.write(nmea, len);
        // Eth_udpPAOGI.endPacket();
    }
}

void ESPGPS::continuousLoop(){
    while (true){
        // myGNSS.update();
        
        while(gpsSerial->available()){
            // Serial.write(gpsSerial->read());
            parser << gpsSerial->read();
        }
        vTaskDelay(10);
    }
}

void ESPGPS::taskHandler(void *param){
    ESPGPS* instance = (ESPGPS*)param;
    instance->continuousLoop();
}

void ESPGPS::sendNTRIP(uint8_t* data, uint8_t len){

    gpsSerial->write(data, len);
}

// MCPManager helper methods
void ESPGPS::updateGPSIndicators() {
    if (mcpManager.isInitialized()) {
        // GPS fix indicator: ON if we have any fix (quality > 0)
        bool hasGPSFix = (atoi(espData->gps.fixQuality) > 0);
        
        // RTK fix indicator: ON if we have RTK fix (quality 4 or 5)
        int quality = atoi(espData->gps.fixQuality);
        bool hasRTKFix = (quality == 4 || quality == 5);
        
        mcpManager.setGPSFix(_gpsFixIndPin, hasGPSFix);
        mcpManager.setRTKFix(_rtkFixIndPin, hasRTKFix);
    }
}

void ESPGPS::setGPSIndicators(bool hasGPSFix, bool hasRTKFix) {
    if (mcpManager.isInitialized()) {
        mcpManager.setGPSFix(_gpsFixIndPin, hasGPSFix);
        mcpManager.setRTKFix(_rtkFixIndPin, hasRTKFix);
    } else {
        Serial.println("GPS: MCPManager not initialized, cannot set indicators");
    }
}

/**
 * @brief Handles IMU (Inertial Measurement Unit) data processing
 * 
 * This function reads orientation data from the BNO08x sensor when the IMU is active.
 * It retrieves yaw, pitch, and roll values, converts them to integer representation
 * by multiplying by 100, and stores them as strings in the GPS data structure.
 * 
 * @details The function only processes data when espData->gps.imuState is 1 (active).
 *          All orientation values are scaled by 100 and converted to string format
 *          for storage in the corresponding IMU data fields.
 * 
 * @note If the RVC sensor read operation fails, the function returns early without
 *       updating any IMU data fields.
 */
void ESPGPS::imuHandler(){
    if (espData->gps.imuState == 1){
        BNO08x_RVC_Data heading;
        if (!rvc.read(&heading)) {
            return;
        }
        int16_t temp = 0;
        temp = heading.yaw * 100;
        itoa(temp, espData->gps.imuHeading, 10);
        temp = heading.pitch * 100;
        itoa(temp, espData->gps.imuPitch, 10);
        temp = heading.roll * 100;
        itoa(temp, espData->gps.imuRoll, 10);
       

    } else {
        // IMU not active, do nothing
        itoa(9999, espData->gps.imuHeading, 10);
        itoa(9999, espData->gps.imuPitch, 10);
        itoa(9999, espData->gps.imuRoll, 10);
        return;
    }
}

