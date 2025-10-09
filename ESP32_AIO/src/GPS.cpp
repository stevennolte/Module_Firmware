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
    : parser(), rvc(), myGNSS(), i2cManager(I2CManager::getInstance()) {
    espData = vars;
    this->gpsSerial = gpsSerial;
    this->bnoSerial = bnoSerial;
    
    _gpsFixIndPin = espData->pins.gpsFix;
    _rtkFixIndPin = espData->pins.rtkFix;
    
    // No need for manual NMEA buffer when using NMEAParser library
    Serial.println("GPS constructor: Using NMEAParser library");
    
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
    // Safety check
    if (!espData) {
        Serial.println("ERROR: espData is null in GGA_Handler");
        return;
    }
    
    Serial.println("got gga");
    
    // Clear GPS data fields first to ensure clean data
    memset(espData->gps.fixTime, 0, sizeof(espData->gps.fixTime));
    memset(espData->gps.latitude, 0, sizeof(espData->gps.latitude));
    memset(espData->gps.latNS, 0, sizeof(espData->gps.latNS));
    memset(espData->gps.longitude, 0, sizeof(espData->gps.longitude));
    memset(espData->gps.lonEW, 0, sizeof(espData->gps.lonEW));
    memset(espData->gps.fixQuality, 0, sizeof(espData->gps.fixQuality));
    memset(espData->gps.numSats, 0, sizeof(espData->gps.numSats));
    memset(espData->gps.HDOP, 0, sizeof(espData->gps.HDOP));
    memset(espData->gps.altitude, 0, sizeof(espData->gps.altitude));
    memset(espData->gps.ageDGPS, 0, sizeof(espData->gps.ageDGPS));
    
    // Parse GPS data with error checking
    try {
        // fix time
        parser.getArg(0, espData->gps.fixTime);

        // latitude
        parser.getArg(1, espData->gps.latitude);
        parser.getArg(2, espData->gps.latNS);

        // longitude
        parser.getArg(3, espData->gps.longitude);
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
        
        Serial.println("GGA parsing completed successfully");
        
        // Only build NMEA sentence if parsing was successful
        buildNmea();
        
    } catch (...) {
        Serial.println("ERROR: Exception in GGA parsing");
    }
}

/**
 * @brief NMEA VTG sentence handler for GPS speed and course data
 * 
 * @details Parses NMEA VTG (Track Made Good and Ground Speed) sentences to extract:
 *          - Course over ground (true and magnetic)
 *          - Speed over ground (knots and km/h)
 * 
 * @note Called automatically by NMEA parser when VTG sentence is received
 */
void ESPGPS::VTG_Handler() 
{
    Serial.println("got vtg");
    
    // Course over ground (true) - field 0
    // parser.getArg(0, espData->gps.courseTrue);
    
    // Course over ground (magnetic) - field 2  
    // parser.getArg(2, espData->gps.courseMagnetic);
    
    // Speed over ground in knots - field 4
    parser.getArg(4, espData->gps.speedKnots);
    
    // Speed over ground in km/h - field 6
    // parser.getArg(6, espData->gps.speedKmh);
    
    // Don't rebuild NMEA here - speed will be included in next GGA sentence
    // This prevents duplicate sentences and improves efficiency
}

void ESPGPS::staticVTG_Handler(){
  if(instance){
    instance->VTG_Handler();
  }
}
void ESPGPS::staticGGA_Handler(){
  if(instance){
    instance->GGA_Handler();
  }
}
void ESPGPS::init(ESPudp* espUdp){
    this->espUdp = espUdp;
    
    // Setup GPS indicators using MCPManager member
    // if (mcpManager.isInitialized()) {
    //     mcpManager.setupGPSIndicators();  // Uses ESPdata pin definitions
    //     mcpManager.testGPSIndicators();   // Uses ESPdata pin definitions
    //     Serial.println("GPS: Indicator pins configured using MCPManager");
    // } else {
    //     Serial.println("GPS: MCPManager not initialized, skipping indicator setup");
    // }
   
    parser.addHandler("GNGGA", staticGGA_Handler);
    parser.addHandler("GNVTG", staticVTG_Handler);  // Add VTG handler for speed data
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
            while (!taskCompleted && (millis() - attemptStart < 5000)) {
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
            // myGNSS.setModeRoverAutomotive();
            float rate = 0.1;
            myGNSS.setNMEAMessage("GPGGA", rate); 
            // myGNSS.setNMEAMessage("GPGSA", rate); 
            // myGNSS.setNMEAMessage("GPGST", rate); 
            // myGNSS.setNMEAMessage("GPRMC", rate); 
            // myGNSS.setNMEAMessage("GPGSV", rate);
            // myGNSS.setNMEAMessage("GNGGA", rate);
            myGNSS.setNMEAMessage("GPVTG", rate);

            // myGNSS.
            myGNSS.saveConfiguration();
        }
        Serial.print("Time to start gps: ");
        Serial.print(millis() - gpsStart);
        Serial.println(" ms");
        
        // Enable GPS task with debugging to see raw data
        Serial.println("Starting GPS task with raw data debugging enabled");
        xTaskCreatePinnedToCore(taskHandler, "GPS_Task", 16000, this, 3, NULL, 1);
    }
    Serial.println("GPS initialization complete");
    // delay(1000);
    
}


void ESPGPS::calculateChecksum(void)
{
  int16_t sum = 0;
  int16_t inx = 0;
  char tmp;

  // Get the actual length of the NMEA string to avoid reading beyond valid data
  int nmeaLength = strlen(espData->gps.nmea);
  
  // Safety check - ensure we don't exceed buffer bounds
  if (nmeaLength >= 150) {
    Serial.println("ERROR: NMEA string too long for checksum calculation");
    return;
  }

  // The checksum calc starts after '$' and ends before '*'
  for (inx = 1; inx < nmeaLength; inx++)
  {
    tmp = espData->gps.nmea[inx];
    
    // * Indicates end of data and start of checksum
    if (tmp == '*')
    {
      break;
    }

    sum ^= tmp;    // Build checksum
  }
  
  // Convert sum to 2-digit hex using simple conversion instead of lookup table
  char checksumStr[3];
  sprintf(checksumStr, "%02X", (unsigned char)sum);
  
  // Append the checksum after the * character
  strcat(espData->gps.nmea, checksumStr);
  
  Serial.print("Calculated checksum: ");
  Serial.println(checksumStr);
}



void ESPGPS::buildNmea()
{
    // Safety check - ensure we have valid data before building NMEA
    if (!espData || !espUdp) {
        Serial.println("ERROR: espData or espUdp is null in buildNmea");
        return;
    }
    
    // Clear the NMEA buffer
    strcpy(espData->gps.nmea, "");
    
    // Build PANDA sentence in AgOpenGPS format
    // $PANDA,time,lat,latNS,lon,lonEW,quality,numSats,hdop,alt,geoidHeight,speedKnots,heading*checksum
    strcat(espData->gps.nmea, "$PANDA,");
    
    // Add fix time (or empty if not available)
    if (strlen(espData->gps.fixTime) > 0) {
        strcat(espData->gps.nmea, espData->gps.fixTime);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add latitude (limit decimal places for AgOpenGPS compatibility)
    if (strlen(espData->gps.latitude) > 0) {
        // Truncate latitude to 10 characters max (reduces precision but fits in buffer)
        char truncLat[12];
        strncpy(truncLat, espData->gps.latitude, 10);
        truncLat[10] = '\0';
        strcat(espData->gps.nmea, truncLat);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add latitude N/S
    if (strlen(espData->gps.latNS) > 0) {
        strcat(espData->gps.nmea, espData->gps.latNS);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add longitude (limit decimal places for AgOpenGPS compatibility)
    if (strlen(espData->gps.longitude) > 0) {
        // Truncate longitude to 11 characters max (reduces precision but fits in buffer)
        char truncLon[13];
        strncpy(truncLon, espData->gps.longitude, 11);
        truncLon[11] = '\0';
        strcat(espData->gps.nmea, truncLon);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add longitude E/W
    if (strlen(espData->gps.lonEW) > 0) {
        strcat(espData->gps.nmea, espData->gps.lonEW);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add fix quality
    if (strlen(espData->gps.fixQuality) > 0) {
        strcat(espData->gps.nmea, espData->gps.fixQuality);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add number of satellites
    if (strlen(espData->gps.numSats) > 0) {
        strcat(espData->gps.nmea, espData->gps.numSats);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add HDOP (or empty)
    if (strlen(espData->gps.HDOP) > 0) {
        // Truncate HDOP to 4 characters max
        char truncHDOP[6];
        strncpy(truncHDOP, espData->gps.HDOP, 4);
        truncHDOP[4] = '\0';
        strcat(espData->gps.nmea, truncHDOP);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add altitude (or empty)
    if (strlen(espData->gps.altitude) > 0) {
        // Truncate altitude to 6 characters max
        char truncAlt[8];
        strncpy(truncAlt, espData->gps.altitude, 6);
        truncAlt[6] = '\0';
        strcat(espData->gps.nmea, truncAlt);
    }
    strcat(espData->gps.nmea, ",");
    
    // Skip geoid height field to save space - leave empty
    strcat(espData->gps.nmea, ",");
    
    // Add speed in knots (truncate to save space)
    if (strlen(espData->gps.speedKnots) > 0) {
        char truncSpeed[6];
        strncpy(truncSpeed, espData->gps.speedKnots, 4);
        truncSpeed[4] = '\0';
        strcat(espData->gps.nmea, truncSpeed);
    }
    strcat(espData->gps.nmea, ",");
    
    // Add heading from IMU (truncate to save space)
    if (strlen(espData->gps.imuHeading) > 0) {
        char truncHeading[6];
        strncpy(truncHeading, espData->gps.imuHeading, 4);
        truncHeading[4] = '\0';
        strcat(espData->gps.nmea, truncHeading);
    }
    
    strcat(espData->gps.nmea, "*");
    
    // Check length before checksum to prevent overflow
    if (strlen(espData->gps.nmea) > 145) {
        Serial.println("WARNING: NMEA sentence too long, truncating");
        espData->gps.nmea[145] = '*';
        espData->gps.nmea[146] = '\0';
    }
    
    // Debug: Show sentence before checksum
    Serial.print("Before checksum: ");
    Serial.println(espData->gps.nmea);
    
    // Calculate and append checksum (this function is now safe with buffer bounds checking)
    calculateChecksum();
    
    // Add proper NMEA line ending
    strcat(espData->gps.nmea, "\r\n");
    
    // Debug: Show complete sentence
    Serial.print("Complete NMEA: ");
    Serial.println(espData->gps.nmea);
    
    // Send via UDP if WiFi is connected
    if (espData->wifi.state == 1)
    {
        Serial.println("Sending GPS via UDP");
        int len = strlen(espData->gps.nmea);
        Serial.print("NMEA Length: ");
        Serial.println(len);
        espUdp->sendUDPgps(espData->gps.nmea);
    }
}void ESPGPS::continuousLoop(){
    uint32_t lastImuCall = 0;
    uint32_t loopCount = 0;
    
    Serial.println("GPS task started - using NMEAParser library");
    
    // No buffer allocation needed for NMEAParser approach
    
    Serial.println("GPS entering main loop...");
    
    while (true){
        loopCount++;
        
        // Memory safety check every 1000 loops
        if (loopCount % 1000 == 0) {
            uint32_t freeHeap = ESP.getFreeHeap();
            if (freeHeap < 20000) {
                Serial.printf("WARNING: Low memory in GPS loop: %d bytes\n", freeHeap);
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait longer if memory is low
                continue;
            }
        }
        
        // Process GPS data using original NMEAParser with fixed checksum function
        int bytesProcessed = 0;
        const int maxBytes = 64; // Conservative but reasonable limit for NMEAParser
        
        while(gpsSerial->available() && bytesProcessed < maxBytes){
            parser << gpsSerial->read();
            bytesProcessed++;
            
            // Use original NMEAParser - this should work now that calculateChecksum is fixed
            // parser << c;
        }
        // Original NMEAParser approach is now active - no longer need manual parsing
        //             if (bufferIndex > 6) { // Minimum NMEA sentence length
        //                 processCompleteSentence(nmeaBuffer);
        //             }
                    
        //             // Reset buffer
        //             bufferIndex = 0;
        //             if (nmeaBuffer) {
        //                 memset(nmeaBuffer, 0, NMEA_BUFFER_SIZE);
        //             }
        //         }
        //     } else {
        //         // Buffer overflow protection
        //         Serial.println("Buffer overflow!");
        //         bufferIndex = 0;
        //         if (nmeaBuffer) {
        //             memset(nmeaBuffer, 0, NMEA_BUFFER_SIZE);
        //         }
        //     }
        // }
        
        
        // IMU handler every 500ms
        uint32_t currentTime = millis();
        if (currentTime - lastImuCall >= 500) {
            imuHandler(); // Enable IMU for heading data in PANDA sentence
            lastImuCall = currentTime;
        }
        
        // Generous delay to prevent overwhelming the system
        vTaskDelay(pdMS_TO_TICKS(100)); // Increased to 100ms
    }
}

// Manual parsing functions - commented out since we're back to using NMEAParser
// void ESPGPS::processCompleteSentence(const char* sentence) {
//     // Safety checks
//     if (!sentence || !nmeaBuffer) return;
    
//     // Count sentences
//     static uint32_t totalSentences = 0;
//     totalSentences++;
    
//     // Only occasionally report that we're receiving data
//     if (totalSentences % 50 == 0) {
//         Serial.printf("Received %d NMEA sentences\n", totalSentences);
//     }
    
//     // SAFE ALTERNATIVE: Parse GPS data manually instead of using problematic parser
//     // This bypasses the NMEAParser library that was causing crashes
    
//     if (sentence[0] == '$' && strlen(sentence) > 10) {
//         // Simple manual parsing for essential GPS data
//         parseNMEAManually(sentence);
//     }
// }

// Manual NMEA parsing function - commented out since we're back to using NMEAParser
// void ESPGPS::parseNMEAManually(const char* sentence) {
//     // Extract sentence type (first 6 characters after $)
//     char sentenceType[7] = {0};
//     strncpy(sentenceType, sentence, 6);
//     sentenceType[6] = '\0';
    
//     // Process GGA sentences for basic position data
//     if (strstr(sentenceType, "GGA") != nullptr) {
//         // Simple comma-separated parsing for GGA sentence
//         // Format: $GPGGA,time,lat,latNS,lon,lonEW,quality,numSats,hdop,alt,altUnit,geoidHeight,geoidUnit,dgpsAge,dgpsID*checksum
        
//         // Count commas to find fields
//         const char* ptr = sentence;
//         int fieldCount = 0;
//         char fieldBuffer[20] = {0};
//         int bufferIndex = 0;
        
//         while (*ptr && fieldCount < 15) {
//             if (*ptr == ',' || *ptr == '*') {
//                 // Process field based on count
//                 fieldBuffer[bufferIndex] = '\0';
                
//                 switch(fieldCount) {
//                     case 1: // Time
//                         strncpy(espData->gps.fixTime, fieldBuffer, sizeof(espData->gps.fixTime) - 1);
//                         break;
//                     case 2: // Latitude
//                         strncpy(espData->gps.latitude, fieldBuffer, sizeof(espData->gps.latitude) - 1);
//                         break;
//                     case 3: // Latitude N/S
//                         strncpy(espData->gps.latNS, fieldBuffer, sizeof(espData->gps.latNS) - 1);
//                         break;
//                     case 4: // Longitude  
//                         strncpy(espData->gps.longitude, fieldBuffer, sizeof(espData->gps.longitude) - 1);
//                         break;
//                     case 5: // Longitude E/W
//                         strncpy(espData->gps.lonEW, fieldBuffer, sizeof(espData->gps.lonEW) - 1);
//                         break;
//                     case 6: // Fix quality
//                         strncpy(espData->gps.fixQuality, fieldBuffer, sizeof(espData->gps.fixQuality) - 1);
//                         espData->gps.fixQualityInt = atoi(fieldBuffer);
//                         break;
//                     case 7: // Number of satellites
//                         strncpy(espData->gps.numSats, fieldBuffer, sizeof(espData->gps.numSats) - 1);
//                         break;
//                     case 8: // HDOP
//                         strncpy(espData->gps.HDOP, fieldBuffer, sizeof(espData->gps.HDOP) - 1);
//                         break;
//                     case 9: // Altitude
//                         strncpy(espData->gps.altitude, fieldBuffer, sizeof(espData->gps.altitude) - 1);
//                         break;
//                 }
                
//                 fieldCount++;
//                 bufferIndex = 0;
//                 memset(fieldBuffer, 0, sizeof(fieldBuffer));
//             } else {
//                 if (bufferIndex < sizeof(fieldBuffer) - 1) {
//                     fieldBuffer[bufferIndex++] = *ptr;
//                 }
//             }
//             ptr++;
//         }
        
//         // Update GPS indicators using I2CManager
//         if (espData->gps.fixQualityInt > 0) {
//             i2cManager.setGPSLED(LEDPattern::ON);
            
//             // RTK fix indication (quality 4 or 5)
//             if (espData->gps.fixQualityInt >= 4) {
//                 i2cManager.setRTKLED(LEDPattern::ON);
//             } else {
//                 i2cManager.setRTKLED(LEDPattern::SLOW_PULSE);
//             }
//         } else {
//             i2cManager.setGPSLED(LEDPattern::FAST_PULSE);
//             i2cManager.setRTKLED(LEDPattern::OFF);
//         }
//     }
// }


void ESPGPS::taskHandler(void *param){
    ESPGPS* instance = (ESPGPS*)param;
    instance->continuousLoop();
}

void ESPGPS::sendNTRIP(uint8_t* data, uint8_t len){

    gpsSerial->write(data, len);
}

// MCPManager helper methods
void ESPGPS::updateGPSIndicators() {
    // if (mcpManager.isInitialized()) {
    //     // GPS fix indicator: ON if we have any fix (quality > 0)
    //     bool hasGPSFix = (atoi(espData->gps.fixQuality) > 0);
        
    //     // RTK fix indicator: ON if we have RTK fix (quality 4 or 5)
    //     int quality = atoi(espData->gps.fixQuality);
    //     bool hasRTKFix = (quality == 4 || quality == 5);
        
    //     mcpManager.setGPSFix(_gpsFixIndPin, hasGPSFix);
    //     mcpManager.setRTKFix(_rtkFixIndPin, hasRTKFix);
    // }
}

void ESPGPS::setGPSIndicators(bool hasGPSFix, bool hasRTKFix) {
    // if (mcpManager.isInitialized()) {
    //     mcpManager.setGPSFix(_gpsFixIndPin, hasGPSFix);
    //     mcpManager.setRTKFix(_rtkFixIndPin, hasRTKFix);
    // } else {
    //     Serial.println("GPS: MCPManager not initialized, cannot set indicators");
    // }
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
    // webSerial("gps called\n");
    // Serial.print("gps called  ");
    // Serial.println(espData->gps.imuState);
    if (espData->gps.imuState == 1){

        BNO08x_RVC_Data heading;
        if (!rvc.read(&heading)) {
            // Serial.println("failed to read");
            return;
        }
        int16_t temp = 0;
        temp = heading.yaw * 100;
        // Serial.println("Yaw: " + String(temp));
        // Serial.println("Pitch: " + String(heading.pitch * 100));
        // Serial.println("Roll: " + String(heading.roll * 100));
        itoa(temp, espData->gps.imuHeading, 10);
        temp = heading.pitch * 100;
        itoa(temp, espData->gps.imuPitch, 10);
        temp = heading.roll * 100;
        itoa(temp, espData->gps.imuRoll, 10);
       

    } else {
        // IMU not active, do nothing
        // itoa(0, espData->gps.imuHeading, 10);
        // itoa(0, espData->gps.imuPitch, 10);
        // itoa(0, espData->gps.imuRoll, 10);
        return;
    }
}

