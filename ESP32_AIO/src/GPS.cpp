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

// #define DEBUG_NTRIP_PRINT

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
    
    // Initialize NMEA message counters in ESPdata struct
    espData->gps.ggaMessageCount = 0;
    espData->gps.vtgMessageCount = 0;
    espData->gps.gsaMessageCount = 0;
    espData->gps.rmcMessageCount = 0;
    espData->gps.otherMessageCount = 0;
    espData->gps.pandaMessageCount = 0;
    espData->gps.lastPandaTime = 0;
    
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
/**
 * @brief Helper function to clean unwanted characters from GPS data fields
 * @param str String to clean
 */
void ESPGPS::cleanDataField(char* str) {
    if (!str) return;
    
    // Remove any \r, \n, or other control characters
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (str[i] == '\r' || str[i] == '\n' || str[i] < 32 || str[i] > 126) {
            str[i] = '\0';  // Terminate string at first bad character
            break;
        }
    }
    
    // Ensure string is properly null-terminated and remove trailing spaces
    if (len > 0) {
        // Remove trailing whitespace
        int end = strlen(str) - 1;
        while (end >= 0 && (str[end] == ' ' || str[end] == '\t')) {
            str[end] = '\0';
            end--;
        }
    }
}

/**
 * @brief NMEA GGA sentence handler for GPS position data
 * 
 * @details Custom parser for GGA sentences to extract:
 *          - Fix time (UTC)
 *          - Latitude and longitude coordinates
 *          - Fix quality indicator
 *          - Number of satellites in use
 *          - Horizontal dilution of precision (HDOP)
 *          - Altitude above mean sea level
 * 
 * @note Called when a complete GGA sentence is received
 */
void ESPGPS::parseGGA(const char* sentence) 
{
    // GGA format: $GPGGA,time,lat,latNS,lon,lonEW,quality,numSats,hdop,alt,altUnit,geoidHeight,geoidUnit,dgpsAge,dgpsID*checksum
    
    char* fields[15];
    int fieldCount = 0;
    char tempSentence[150];
    strcpy(tempSentence, sentence);
    
    // Split sentence by commas
    char* token = strtok(tempSentence, ",*");
    while (token != NULL && fieldCount < 15) {
        fields[fieldCount++] = token;
        token = strtok(NULL, ",*");
    }
    
    if (fieldCount >= 10) {
        // Initialize all GPS fields to empty strings first to avoid garbage characters
        espData->gps.fixTime[0] = '\0';
        espData->gps.latitude[0] = '\0';
        espData->gps.latNS[0] = '\0';
        espData->gps.longitude[0] = '\0';
        espData->gps.lonEW[0] = '\0';
        espData->gps.fixQuality[0] = '\0';
        espData->gps.numSats[0] = '\0';
        espData->gps.HDOP[0] = '\0';
        espData->gps.altitude[0] = '\0';
        espData->gps.ageDGPS[0] = '\0';
        
        // Extract time (field 1) and clean it
        if (strlen(fields[1]) > 0) {
            strncpy(espData->gps.fixTime, fields[1], sizeof(espData->gps.fixTime) - 1);
            espData->gps.fixTime[sizeof(espData->gps.fixTime) - 1] = '\0';
            cleanDataField(espData->gps.fixTime);
        }
        
        // Extract latitude (fields 2,3) and clean them
        if (strlen(fields[2]) > 0) {
            strncpy(espData->gps.latitude, fields[2], sizeof(espData->gps.latitude) - 1);
            espData->gps.latitude[sizeof(espData->gps.latitude) - 1] = '\0';
            cleanDataField(espData->gps.latitude);
        }
        if (strlen(fields[3]) > 0) {
            strncpy(espData->gps.latNS, fields[3], sizeof(espData->gps.latNS) - 1);
            espData->gps.latNS[sizeof(espData->gps.latNS) - 1] = '\0';
            cleanDataField(espData->gps.latNS);
        }
        
        // Extract longitude (fields 4,5) and clean them
        if (strlen(fields[4]) > 0) {
            strncpy(espData->gps.longitude, fields[4], sizeof(espData->gps.longitude) - 1);
            espData->gps.longitude[sizeof(espData->gps.longitude) - 1] = '\0';
            cleanDataField(espData->gps.longitude);
        }
        if (strlen(fields[5]) > 0) {
            strncpy(espData->gps.lonEW, fields[5], sizeof(espData->gps.lonEW) - 1);
            espData->gps.lonEW[sizeof(espData->gps.lonEW) - 1] = '\0';
            cleanDataField(espData->gps.lonEW);
        }
        
        // Extract fix quality (field 6) and clean it
        if (strlen(fields[6]) > 0) {
            strncpy(espData->gps.fixQuality, fields[6], sizeof(espData->gps.fixQuality) - 1);
            espData->gps.fixQuality[sizeof(espData->gps.fixQuality) - 1] = '\0';
            cleanDataField(espData->gps.fixQuality);
        }
        
        // Extract number of satellites (field 7) and clean it
        if (strlen(fields[7]) > 0) {
            strncpy(espData->gps.numSats, fields[7], sizeof(espData->gps.numSats) - 1);
            espData->gps.numSats[sizeof(espData->gps.numSats) - 1] = '\0';
            cleanDataField(espData->gps.numSats);
        }
        
        // Extract HDOP (field 8) and clean it
        if (strlen(fields[8]) > 0) {
            strncpy(espData->gps.HDOP, fields[8], sizeof(espData->gps.HDOP) - 1);
            espData->gps.HDOP[sizeof(espData->gps.HDOP) - 1] = '\0';
            cleanDataField(espData->gps.HDOP);
        }
        
        // Extract altitude (field 9) and clean it
        if (strlen(fields[9]) > 0) {
            strncpy(espData->gps.altitude, fields[9], sizeof(espData->gps.altitude) - 1);
            espData->gps.altitude[sizeof(espData->gps.altitude) - 1] = '\0';
            cleanDataField(espData->gps.altitude);
        }
        
        // Set DGPS age to 0 for now (instead of parsing from NMEA)
        strcpy(espData->gps.ageDGPS, "0");
        
        // Increment GGA message counter in ESPdata
        espData->gps.ggaMessageCount++;
        
        // Serial.println("GGA parsed successfully");
    }
}

/**
 * @brief Parse VTG sentence for speed data
 */
void ESPGPS::parseVTG(const char* sentence) 
{
    // VTG format: $GPVTG,courseTrue,T,courseMag,M,speedKnots,N,speedKmh,K,mode*checksum
    
    // Debug occasionally to avoid spam
    static int vtgDebugCounter = 0;
    if (++vtgDebugCounter >= 50) {
        vtgDebugCounter = 0;
        Serial.print("DEBUG VTG: ");
        Serial.println(sentence);
    }
    
    char* fields[10];
    int fieldCount = 0;
    char tempSentence[150];
    strcpy(tempSentence, sentence);
    
    // Split sentence by commas
    char* token = strtok(tempSentence, ",*");
    while (token != NULL && fieldCount < 10) {
        fields[fieldCount++] = token;
        token = strtok(NULL, ",*");
    }
    
    if (vtgDebugCounter == 1) {  // Only show field breakdown occasionally
        Serial.print("DEBUG VTG fields: ");
        for (int i = 0; i < fieldCount; i++) {
            Serial.print("[");
            Serial.print(i);
            Serial.print("]=");
            Serial.print(fields[i]);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    if (fieldCount >= 6) {
        // Initialize fields to empty strings first
        espData->gps.speedKnots[0] = '\0';
        espData->gps.vtgHeading[0] = '\0';
        
        // Extract course/heading in degrees true (field 1) and clean it
        if (fieldCount > 1 && strlen(fields[1]) > 0) {
            strncpy(espData->gps.vtgHeading, fields[1], sizeof(espData->gps.vtgHeading) - 1);
            espData->gps.vtgHeading[sizeof(espData->gps.vtgHeading) - 1] = '\0';
            cleanDataField(espData->gps.vtgHeading);
            if (vtgDebugCounter == 1) {
                Serial.print("DEBUG vtgHeading set to: '");
                Serial.print(espData->gps.vtgHeading);
                Serial.println("'");
            }
        }
        
        // Extract speed in knots (field 5) and clean it
        if (fieldCount > 5 && strlen(fields[5]) > 0) {
            strncpy(espData->gps.speedKnots, fields[5], sizeof(espData->gps.speedKnots) - 1);
            espData->gps.speedKnots[sizeof(espData->gps.speedKnots) - 1] = '\0';
            cleanDataField(espData->gps.speedKnots);
            if (vtgDebugCounter == 1) {
                Serial.print("DEBUG speedKnots set to: '");
                Serial.print(espData->gps.speedKnots);
                Serial.println("'");
            }
        }
        
        // Increment VTG message counter in ESPdata
        espData->gps.vtgMessageCount++;
        
        // Serial.println("VTG parsed successfully");
    } else {
        Serial.println("DEBUG VTG: Not enough fields");
    }
}

/**
 * @brief Custom NMEA parser - replaces NMEAParser library
 */
void ESPGPS::parseNMEASentence(const char* sentence) 
{
    if (!sentence || strlen(sentence) < 7) return;
    
    // Check sentence type and handle based on PANDA mode setting
    if (strstr(sentence, "GGA") != NULL) {
        // Parse GGA sentence to extract GPS data
        parseGGA(sentence);
        espData->gps.ggaMessageCount++;
        
        // Check mode: PANDA generation or raw forwarding
        if (espData->gps.ntripPandaMode) {
            // PANDA mode: build and send PANDA sentence (only for GGA)
            Serial.println("GGA received - generating PANDA sentence");
            buildPandaSentence();
        } else {
            // Raw forwarding mode: send original NMEA sentence
            if (espUdp) {
                espUdp->sendUDPgps(sentence);
            }
        }
    }
    else if (strstr(sentence, "VTG") != NULL) {
        // Parse VTG sentence to extract velocity data
        parseVTG(sentence);
        espData->gps.vtgMessageCount++;
        
        // In PANDA mode, VTG data is used in GGA-triggered PANDA messages
        // In raw mode, forward the VTG sentence
        if (!espData->gps.ntripPandaMode) {
            // Raw forwarding mode: send original NMEA sentence
            if (espUdp) {
                espUdp->sendUDPgps(sentence);
            }
        }
        // Note: VTG data is parsed and stored for use in next GGA-triggered PANDA message
    }
    // Add GSA and RMC parsers here if needed
    else if (strstr(sentence, "GSA") != NULL) {
        // GSA parsing for satellite info if needed
        // Serial.println("GSA sentence received (not parsed yet)");
        if (espUdp){
            espUdp->sendUDPgps(sentence);
        }
        espData->gps.gsaMessageCount++;
    }
    else if (strstr(sentence, "RMC") != NULL) {
        // RMC parsing for additional data if needed
        // Serial.println("RMC sentence received (not parsed yet)");
        if (espUdp){
            espUdp->sendUDPgps(sentence);
        }
        espData->gps.rmcMessageCount++;
    }
    else {
        // Count other message types
        espData->gps.otherMessageCount++;
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
    // Serial.println("got vtg");
    
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
    
    // Initialize all GPS data fields to empty strings to prevent garbage characters
    espData->gps.fixTime[0] = '\0';
    espData->gps.latitude[0] = '\0';
    espData->gps.latNS[0] = '\0';
    espData->gps.longitude[0] = '\0';
    espData->gps.lonEW[0] = '\0';
    espData->gps.fixQuality[0] = '\0';
    espData->gps.numSats[0] = '\0';
    espData->gps.HDOP[0] = '\0';
    espData->gps.altitude[0] = '\0';
    espData->gps.ageDGPS[0] = '\0';
    espData->gps.speedKnots[0] = '\0';
    espData->gps.vtgHeading[0] = '\0';
    espData->gps.imuHeading[0] = '\0';
    espData->gps.imuRoll[0] = '\0';
    espData->gps.imuPitch[0] = '\0';
    espData->gps.imuYawRate[0] = '\0';
    espData->gps.nmea[0] = '\0';
    
    Serial.println("GPS: All data fields initialized to empty strings");
    
    // Setup GPS indicators using MCPManager member
    // if (mcpManager.isInitialized()) {
    //     mcpManager.setupGPSIndicators();  // Uses ESPdata pin definitions
    //     mcpManager.testGPSIndicators();   // Uses ESPdata pin definitions
    //     Serial.println("GPS: Indicator pins configured using MCPManager");
    // } else {
    //     Serial.println("GPS: MCPManager not initialized, skipping indicator setup");
    // }
   
    // Using custom NMEA parser - no need for NMEAParser library handlers
    // parser.addHandler("GNGGA", staticGGA_Handler);
    // parser.addHandler("GNVTG", staticVTG_Handler);
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
            i2cManager.setGPSLED(LEDPattern::ON);
            espData->gps.state = 1;
            Serial.println("UM980 detected!");
        }
        myGNSS.sendCommand("CONFIG RTK RELIABILITY 3 1");
        delay(500);
        myGNSS.sendCommand("CONFIG SMOOTH RTKHEIGHT 0");
        delay(500);
        myGNSS.sendCommand("CONFIG HEADING RELIABILITY 3");
        delay(500);
        myGNSS.sendCommand("CONFIG HEADING VARIABLELENGTH");
        delay(500);
        myGNSS.sendCommand("CONFIG SMOOTH HEADING 0");
        delay(500);
        myGNSS.sendCommand("GNGGA, .1");
        delay(500);
        myGNSS.sendCommand("GPVTG, .1");
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
  
  // Properly append the checksum after the * character
  // The * should already be there, so we just add the checksum
  strcat(espData->gps.nmea, checksumStr);
  
  // Serial.print("Calculated checksum: ");
  // Serial.println(checksumStr);
  // Serial.print("Final sentence after checksum: ");
  // Serial.println(espData->gps.nmea);
}



/**
 * @brief Build PANDA sentence according to AgOpenGPS specification
 */
void ESPGPS::buildPandaSentence()
{
    // Safety check - ensure we have valid data before building NMEA
    if (!espData || !espUdp) {
        Serial.println("ERROR: espData or espUdp is null in buildPandaSentence");
        return;
    }
    
    // Debug: Show what data we have before building sentence (reduce frequency)
    static int pandaDebugCounter = 0;
    if (++pandaDebugCounter >= 10) {
        pandaDebugCounter = 0;
        Serial.println("=== GPS Data Before Building PANDA ===");
        Serial.printf("Time: '%s'\n", espData->gps.fixTime);
        Serial.printf("Lat: '%s' %s\n", espData->gps.latitude, espData->gps.latNS);
        Serial.printf("Lon: '%s' %s\n", espData->gps.longitude, espData->gps.lonEW);
        Serial.printf("Quality: '%s'\n", espData->gps.fixQuality);
        Serial.printf("Sats: '%s'\n", espData->gps.numSats);
        Serial.printf("HDOP: '%s'\n", espData->gps.HDOP);
        Serial.printf("Alt: '%s'\n", espData->gps.altitude);
        Serial.printf("DGPS Age: '%s'\n", espData->gps.ageDGPS);
        Serial.printf("Speed: '%s'\n", espData->gps.speedKnots);
        Serial.printf("IMU Heading: '%s'\n", espData->gps.imuHeading);
        Serial.printf("IMU Roll: '%s'\n", espData->gps.imuRoll);
        Serial.printf("IMU Pitch: '%s'\n", espData->gps.imuPitch);
        Serial.printf("IMU Yaw Rate: '%s'\n", espData->gps.imuYawRate);
        Serial.println("=====================================");
    }
    
    // Clear the NMEA buffer
    strcpy(espData->gps.nmea, "");
    
    // Build PANDA sentence: $PANDA,time,lat,latNS,lon,lonEW,quality,numSats,hdop,alt,dgpsAge,speed,heading,roll,pitch,yawRate*checksum
    strcat(espData->gps.nmea, "$PANDA,");
    
    // (1) Time of fix - only add if we have valid data
    if (strlen(espData->gps.fixTime) > 0 && espData->gps.fixTime[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.fixTime);
    }
    strcat(espData->gps.nmea, ",");
    
    // (2) Latitude - only add if we have valid data
    if (strlen(espData->gps.latitude) > 0 && espData->gps.latitude[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.latitude);
    }
    strcat(espData->gps.nmea, ",");
    
    // (3) Latitude N/S - only add if we have valid data
    if (strlen(espData->gps.latNS) > 0 && espData->gps.latNS[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.latNS);
    }
    strcat(espData->gps.nmea, ",");
    
    // (4) Longitude - only add if we have valid data
    if (strlen(espData->gps.longitude) > 0 && espData->gps.longitude[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.longitude);
    }
    strcat(espData->gps.nmea, ",");
    
    // (5) Longitude E/W - only add if we have valid data
    if (strlen(espData->gps.lonEW) > 0 && espData->gps.lonEW[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.lonEW);
    }
    strcat(espData->gps.nmea, ",");
    
    // (6) Fix quality (0-8 as per specification) - only add if we have valid data
    if (strlen(espData->gps.fixQuality) > 0 && espData->gps.fixQuality[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.fixQuality);
    }
    strcat(espData->gps.nmea, ",");
    
    // (7) Number of satellites being tracked - only add if we have valid data
    if (strlen(espData->gps.numSats) > 0 && espData->gps.numSats[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.numSats);
    }
    strcat(espData->gps.nmea, ",");
    
    // (8) Horizontal dilution of position - only add if we have valid data
    if (strlen(espData->gps.HDOP) > 0 && espData->gps.HDOP[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.HDOP);
    }
    strcat(espData->gps.nmea, ",");
    
    // (9) Altitude (ALWAYS in Meters, above mean sea level) - only add if we have valid data
    if (strlen(espData->gps.altitude) > 0 && espData->gps.altitude[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.altitude);
    }
    strcat(espData->gps.nmea, ",");
    
    // (10) Time in seconds since last DGPS update - set to 0 for now
    strcat(espData->gps.nmea, espData->gps.ageDGPS);
    strcat(espData->gps.nmea, ",");
    
    // (11) Speed in knots - only add if we have valid data
    if (strlen(espData->gps.speedKnots) > 0 && espData->gps.speedKnots[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.speedKnots);
    }
    strcat(espData->gps.nmea, ",");
    
    // FROM IMU:
    // (12) Heading in degrees - only add if we have valid data
    if (strlen(espData->gps.imuHeading) > 0 && espData->gps.imuHeading[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.imuHeading);
    }
    strcat(espData->gps.nmea, ",");
    
    // (13) Roll angle in degrees (positive roll = right leaning) - only add if we have valid data
    if (strlen(espData->gps.imuRoll) > 0 && espData->gps.imuRoll[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.imuRoll);
    }
    strcat(espData->gps.nmea, ",");
    
    // (14) Pitch angle in degrees (positive pitch = nose up) - only add if we have valid data
    if (strlen(espData->gps.imuPitch) > 0 && espData->gps.imuPitch[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.imuPitch);
    }
    strcat(espData->gps.nmea, ",");
    
    // (15) Yaw Rate in Degrees/second - only add if we have valid data
    if (strlen(espData->gps.imuYawRate) > 0 && espData->gps.imuYawRate[0] != '\0') {
        strcat(espData->gps.nmea, espData->gps.imuYawRate);
    }
    
    // Add comma after the last field, then asterisk for checksum
    strcat(espData->gps.nmea, ",*");
    
    // Check length before checksum to prevent overflow
    if (strlen(espData->gps.nmea) > 145) {
        Serial.println("WARNING: PANDA sentence too long, truncating");
        espData->gps.nmea[145] = '*';
        espData->gps.nmea[146] = '\0';
    }
    
    // Debug: Show sentence before checksum
    // Serial.print("Before checksum: ");
    // Serial.println(espData->gps.nmea);
    // Serial.print("Length before checksum: ");
    // Serial.println(strlen(espData->gps.nmea));
    
    // Calculate and append checksum
    calculateChecksum();
    
    // Debug: Show sentence after checksum but before line ending
    // Serial.print("After checksum, before \\r\\n: ");
    // Serial.println(espData->gps.nmea);
    // Serial.print("Length after checksum: ");
    // Serial.println(strlen(espData->gps.nmea));
    
    // Add proper NMEA line ending
    strcat(espData->gps.nmea, "\r\n");
    
    // Debug: Show complete sentence (reduce frequency)
    static int pandaOutputCounter = 0;
    if (++pandaOutputCounter >= 10) {
        pandaOutputCounter = 0;
        Serial.print("Complete PANDA: ");
        Serial.println(espData->gps.nmea);
    }
    
    // Send via UDP if WiFi is connected
    if (espData->wifi.state == 1)
    {
        if (pandaOutputCounter == 1) { // Only show sending message when we show the sentence
            Serial.println("Sending PANDA via UDP");
            int len = strlen(espData->gps.nmea);
            Serial.print("PANDA Length: ");
            Serial.println(len);
        }
        espUdp->sendUDPgps(espData->gps.nmea);
        espData->gps.pandaMessageCount++; // Increment PANDA message counter
        espData->gps.lastPandaTime = millis(); // Record timestamp
    } else {
        Serial.println("WiFi not connected - PANDA not sent");
    }
}void ESPGPS::continuousLoop(){
    uint32_t lastImuCall = 0;
    uint32_t loopCount = 0;
    
    // Serial.println("GPS task started - using NMEAParser library");
    
    // No buffer allocation needed for NMEAParser approach
    
    // Serial.println("GPS entering main loop...");
    
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
        
        // Process GPS data using custom NMEA parser
        int bytesProcessed = 0;
        const int maxBytes = 64; // Conservative limit
        static char nmeaBuffer[200]; // Buffer for building complete sentences
        static int bufferIndex = 0;
        
        while(gpsSerial->available() && bytesProcessed < maxBytes){
            char c = gpsSerial->read();
            bytesProcessed++;
            
            // Build complete NMEA sentences character by character
            if (bufferIndex < sizeof(nmeaBuffer) - 1) {
                nmeaBuffer[bufferIndex++] = c;
                
                // Check for end of NMEA sentence
                if (c == '\n') {
                    nmeaBuffer[bufferIndex] = '\0';
                    
                    // Print raw NMEA string to serial terminal
                    if (bufferIndex > 6) { // Minimum NMEA sentence length
                        // Serial.print("RAW NMEA: ");
                        // Serial.print(nmeaBuffer);
                        parseNMEASentence(nmeaBuffer);
                    }
                    
                    // Reset buffer for next sentence
                    bufferIndex = 0;
                    memset(nmeaBuffer, 0, sizeof(nmeaBuffer));
                }
            } else {
                // Buffer overflow protection - reset
                // Serial.println("NMEA buffer overflow, resetting");
                bufferIndex = 0;
                memset(nmeaBuffer, 0, sizeof(nmeaBuffer));
            }
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
            // imuHandler(); // Enable IMU for heading data in PANDA sentence
            lastImuCall = currentTime;
        }
        imuHandler();
        // Generous delay to prevent overwhelming the system
        vTaskDelay(pdMS_TO_TICKS(5)); // Increased to 100ms
    }
}


void ESPGPS::taskHandler(void *param){
    ESPGPS* instance = (ESPGPS*)param;
    instance->continuousLoop();
}

void ESPGPS::sendNTRIP(uint8_t* data, size_t len){

    // Serial.print("Raw UDP packet sent to GPS: ");
    // Serial.print(len);
    // Serial.print(" bytes - ");
    // for(size_t i = 0; i < min(len, (size_t)32); i++){
    //     Serial.printf("%02X ", data[i]);
    // }
    // if (len > 32) Serial.print("...");
    // Serial.println();
    
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

/**
 * @brief Validate and clean IMU data string
 * @param str String to validate and clean
 * @param maxLen Maximum length of the string
 */
void ESPGPS::validateIMUString(char* str, size_t maxLen) {
    if (!str) return;
    
    // Check for null characters or invalid data in the middle of the string
    for (size_t i = 0; i < maxLen - 1 && str[i] != '\0'; i++) {
        if (str[i] < 32 || str[i] > 126) {
            // Found invalid character, truncate string here
            str[i] = '\0';
            break;
        }
    }
    
    // If string is empty or invalid, set to "0"
    if (strlen(str) == 0) {
        strcpy(str, "0");
    }
    
    // Ensure null termination
    str[maxLen - 1] = '\0';
}

void ESPGPS::imuHandler(){
    // webSerial("gps called\n");
    // Serial.print("gps called  ");
    // Serial.println(espData->gps.imuState);
    
    if (true){

        BNO08x_RVC_Data heading;
        if (!rvc.read(&heading)) {
            // No new data available - this is normal for RVC mode
            // Keep existing IMU values, don't reset to 0
            espData->gps.imuState = 1; // IMU is still active
            
            if (millis() - espData->gps.imuMessageTime > 1000){
                espData->gps.imuState = 2; // Set to error state after 1 second of no data
            } 
            return;
        }
        espData->gps.imuState = 1; // Ensure state is active
        espData->gps.imuMessageTime = millis();
        
        // Create temporary variables to avoid partial updates
        char tempHeading[6], tempPitch[6], tempRoll[6];
        memset(tempHeading, 0, sizeof(tempHeading));
        memset(tempPitch, 0, sizeof(tempPitch));
        memset(tempRoll, 0, sizeof(tempRoll));
        
        // Convert values to strings in temporary buffers first
        int16_t temp = heading.yaw * 100;
        snprintf(tempHeading, sizeof(tempHeading), "%d", temp);
        
        temp = heading.pitch * 100;
        snprintf(tempPitch, sizeof(tempPitch), "%d", temp);
        
        temp = heading.roll * 100;
        snprintf(tempRoll, sizeof(tempRoll), "%d", temp);
        
        // Validate the strings don't contain invalid characters
        for (int i = 0; i < strlen(tempHeading); i++) {
            if (tempHeading[i] < 32 || tempHeading[i] > 126) tempHeading[i] = '0';
        }
        for (int i = 0; i < strlen(tempPitch); i++) {
            if (tempPitch[i] < 32 || tempPitch[i] > 126) tempPitch[i] = '0';
        }
        for (int i = 0; i < strlen(tempRoll); i++) {
            if (tempRoll[i] < 32 || tempRoll[i] > 126) tempRoll[i] = '0';
        }
        
        // Now atomically copy to the actual fields
        memset(espData->gps.imuHeading, 0, sizeof(espData->gps.imuHeading));
        memset(espData->gps.imuPitch, 0, sizeof(espData->gps.imuPitch));
        memset(espData->gps.imuRoll, 0, sizeof(espData->gps.imuRoll));
        memset(espData->gps.imuYawRate, 0, sizeof(espData->gps.imuYawRate));
        
        strcpy(espData->gps.imuHeading, tempHeading);
        strcpy(espData->gps.imuPitch, tempPitch);
        strcpy(espData->gps.imuRoll, tempRoll);
        strcpy(espData->gps.imuYawRate, "0");
        
        // Validate all IMU strings to ensure they're clean
        validateIMUString(espData->gps.imuHeading, sizeof(espData->gps.imuHeading));
        validateIMUString(espData->gps.imuPitch, sizeof(espData->gps.imuPitch));
        validateIMUString(espData->gps.imuRoll, sizeof(espData->gps.imuRoll));
        validateIMUString(espData->gps.imuYawRate, sizeof(espData->gps.imuYawRate));
        
        // Debug output occasionally to avoid spam (every 100 readings)
        static int debugCounter = 0;
        if (++debugCounter >= 100) {
            debugCounter = 0;
            Serial.print("DEBUG IMU: H='");
            Serial.print(espData->gps.imuHeading);
            Serial.print("' P='");
            Serial.print(espData->gps.imuPitch);
            Serial.print("' R='");
            Serial.print(espData->gps.imuRoll);
            Serial.println("'");
        }

    } 
    // Note: IMU is currently always enabled (if condition above is 'true')
    // The following else block would only execute if IMU was conditionally disabled
    /*
    else {
        // IMU not active, set all values to "0" and clear any garbage
        memset(espData->gps.imuHeading, 0, sizeof(espData->gps.imuHeading));
        memset(espData->gps.imuPitch, 0, sizeof(espData->gps.imuPitch));
        memset(espData->gps.imuRoll, 0, sizeof(espData->gps.imuRoll));
        memset(espData->gps.imuYawRate, 0, sizeof(espData->gps.imuYawRate));
        
        strcpy(espData->gps.imuHeading, "0");
        strcpy(espData->gps.imuPitch, "0");
        strcpy(espData->gps.imuRoll, "0");
        strcpy(espData->gps.imuYawRate, "0");
        
        // Validate all IMU strings even when inactive
        validateIMUString(espData->gps.imuHeading, sizeof(espData->gps.imuHeading));
        validateIMUString(espData->gps.imuPitch, sizeof(espData->gps.imuPitch));
        validateIMUString(espData->gps.imuRoll, sizeof(espData->gps.imuRoll));
        validateIMUString(espData->gps.imuYawRate, sizeof(espData->gps.imuYawRate));
        
        return;
    }
    */
}

/**
 * @brief Display current NMEA message counts
 */
void ESPGPS::logMessageCounts() 
{
    Serial.println("=== NMEA Message Counts ===");
    Serial.printf("GGA messages: %lu\n", espData->gps.ggaMessageCount);
    Serial.printf("VTG messages: %lu\n", espData->gps.vtgMessageCount);
    Serial.printf("GSA messages: %lu\n", espData->gps.gsaMessageCount);
    Serial.printf("RMC messages: %lu\n", espData->gps.rmcMessageCount);
    Serial.printf("Other messages: %lu\n", espData->gps.otherMessageCount);
    uint32_t total = espData->gps.ggaMessageCount + espData->gps.vtgMessageCount + 
                     espData->gps.gsaMessageCount + espData->gps.rmcMessageCount + 
                     espData->gps.otherMessageCount;
    Serial.printf("Total messages: %lu\n", total);
    Serial.println("===========================");
}

/**
 * @brief Reset all NMEA message counters to zero
 */
void ESPGPS::resetMessageCounts() 
{
    espData->gps.ggaMessageCount = 0;
    espData->gps.vtgMessageCount = 0;
    espData->gps.gsaMessageCount = 0;
    espData->gps.rmcMessageCount = 0;
    espData->gps.otherMessageCount = 0;
    Serial.println("NMEA message counters reset to zero");
}

