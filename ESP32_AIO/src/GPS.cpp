#include "GPS.h"

ESPGPS* ESPGPS::instance = nullptr;

// New constructor using MCPManager singleton (no MCP pointer needed)
ESPGPS::ESPGPS(ESPdata* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial) 
    : parser(), rvc(), myGNSS(), mcpManager(MCPManager::getInstance()) {
    espData = vars;
    this->gpsSerial = gpsSerial;
    this->bnoSerial = bnoSerial;
    
    _gpsFixIndPin = espData->pins.gpsFix;
    _rtkFixIndPin = espData->pins.rtkFix;
    instance = this;
}



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
    if (!espData.gps.externalGPS){
        Serial.println("Trying to start UM980");
        uint8_t gpsTryCnt = 0;
        while (myGNSS.begin(*gpsSerial) == false && gpsTryCnt < 2){
            gpsTryCnt++;
            delay(250);
            Serial.print(".");
            
        }
        Serial.println();
        if (!myGNSS.isConnected()) //Give the serial port over to the library
        {
            espData->gps.state = 2;
            Serial.println("UM980 failed to respond. Check ports and baud rates.");
            
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
    
        xTaskCreatePinnedToCore(taskHandler, "taskHandler", 10000, this, 1, NULL, 0);
    }
    
    delay(1000);
    
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


