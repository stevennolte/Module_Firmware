#include "GPS.h"

GPS* GPS::instance = nullptr;

// New constructor using MCPManager singleton (no MCP pointer needed)
GPS::GPS(ESPdata* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial) : parser(), rvc() , myGNSS(){
    espData = vars;
    
    this->gpsSerial = gpsSerial;
    this->bnoSerial = bnoSerial;
    
    _gpsFixIndPin = espData->gpioDefs.gpsFix;
    _rtkFixIndPin = espData->gpioDefs.rtkFix;
    instance = this;
}



void GPS::GGA_Handler() //Rec'd GGA
{
    // fix time
    parser.getArg(0, espData->gpsData.fixTime);

    // latitude
    parser.getArg(1, espData->gpsData.latitude);
    parser.getArg(2, espData->gpsData.latNS);

    // longitude
    parser.getArg(3, espData->gpsData.longitude);
    // Serial.println(longitude);
    parser.getArg(4, espData->gpsData.lonEW);

    // fix quality
    parser.getArg(5, espData->gpsData.fixQuality);

    // satellite #
    parser.getArg(6, espData->gpsData.numSats);

    // HDOP
    parser.getArg(7, espData->gpsData.HDOP);

    // altitude
    parser.getArg(8, espData->gpsData.altitude);

    // time of last DGPS update
    parser.getArg(12, espData->gpsData.ageDGPS);

    buildNmea();
}
void GPS::staticGGA_Handler(){
  if(instance){
    instance->GGA_Handler();
  }
}
void GPS::init(ESPudp* espUdp){
    this->espUdp = espUdp;
    
    // Use MCPManager if no MCP pointer was provided in constructor
    
    MCPManager& mcpManager = MCPManager::getInstance();
    if (mcpManager.isInitialized()) {
        mcpManager.setGPSactive();
    }
   
    parser.addHandler("G-GGA", staticGGA_Handler);
    // parser.setErrorHandler(errorHandler);
    // parser.addHandler("G-GGA", GPS::GGA_Handler);
    // parser.addHandler("G-VTG", VTG_Handler);
    // if (!rvc.begin(&bnoSerial)){
    //     Serial.println("BNO08x not detected");
    // }
    if (rvc.begin(bnoSerial)){
        espData->gpsData.imuState = 1;
    } else {
        espData->gpsData.imuState = 2;
        Serial.println("RVC Start Failed");
    }
    
    uint8_t gpsTryCnt = 0;
    while (myGNSS.begin(*gpsSerial) == false && gpsTryCnt < 5){
        gpsTryCnt++;
        delay(250);
        Serial.println("Trying to start UM980");
    }
    if (!myGNSS.isConnected()) //Give the serial port over to the library
    {
        espData->gpsData.state = 2;
        Serial.println("UM980 failed to respond. Check ports and baud rates. Freezing...");
        
    } else {
        espData->gpsData.state = 1;
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
        xTaskCreatePinnedToCore(taskHandler, "taskHandler", 10000, this, 1, NULL, 0);
    }
    
    delay(1000);
    
}

// Alternative method using MCPManager singleton
void GPS::initWithSingleton(ESPudp* espUdp){
    this->espUdp = espUdp;
    
    // Use MCPManager singleton instead of mcp pointer
    MCPManager& mcpManager = MCPManager::getInstance();
    
    if (mcpManager.isInitialized()) {
        mcpManager.pinMode(_gpsFixIndPin, OUTPUT);
        mcpManager.pinMode(_rtkFixIndPin, OUTPUT);
        mcpManager.digitalWrite(_gpsFixIndPin, HIGH);
        mcpManager.digitalWrite(_rtkFixIndPin, HIGH);
        delay(1000);
        mcpManager.digitalWrite(_gpsFixIndPin, LOW);
        mcpManager.digitalWrite(_rtkFixIndPin, LOW);
        
        Serial.println("GPS: Using MCPManager singleton for pin control");
    } else {
        Serial.println("GPS: MCPManager not initialized, skipping pin setup");
    }
    
    // Rest of initialization is the same as original init method
    parser.addHandler("G-GGA", staticGGA_Handler);
    
    if (rvc.begin(bnoSerial)){
        espData->gpsData.imuState = 1;
    } else {
        espData->gpsData.imuState = 2;
        Serial.println("RVC Start Failed");
    }
    
    uint8_t gpsTryCnt = 0;
    while (myGNSS.begin(*gpsSerial) == false && gpsTryCnt < 5){
        gpsTryCnt++;
        delay(250);
        Serial.println("Trying to start UM980");
    }
    if (!myGNSS.isConnected()) {
        espData->gpsData.state = 2;
        Serial.print("UM980 Failed to Respond on Serial, State: ");
        Serial.println(espData->gpsData.state);
    } else {
        Serial.println("UM980 Connected and ready");
        espData->gpsData.state = 1;
        xTaskCreatePinnedToCore(taskHandler, "taskHandler", 10000, this, 1, NULL, 0);
    }
    
    delay(1000);
}

void GPS::calculateChecksum(void)
{
  int16_t sum = 0;
  int16_t inx = 0;
  char tmp;

  // The checksum calc starts after '$' and ends before '*'
  for (inx = 1; inx < 200; inx++)
  {
    tmp = espData->gpsData.nmea[inx];

    // * Indicates end of data and start of checksum
    if (tmp == '*')
    {
      break;
    }

    sum ^= tmp;    // Build checksum
  }

  byte chk = (sum >> 4);
  char hex[2] = { espData->gpsData.asciiHex[chk], 0 };
  strcat(espData->gpsData.nmea, hex);

  chk = (sum % 16);
  char hex2[2] = { espData->gpsData.asciiHex[chk], 0 };
  strcat(espData->gpsData.nmea, hex2);
}



void GPS::buildNmea()
{
    strcpy(espData->gpsData.nmea, "");
    strcat(espData->gpsData.nmea, "$PANDA,");
    strcat(espData->gpsData.nmea, espData->gpsData.fixTime);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.latitude);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.latNS);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.longitude);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.lonEW);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.fixQuality);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.numSats);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.HDOP);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.altitude);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.ageDGPS);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.speedKnots);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.imuHeading);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.imuRoll);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.imuPitch);
    strcat(espData->gpsData.nmea, ",");
    strcat(espData->gpsData.nmea, espData->gpsData.imuYawRate);
    strcat(espData->gpsData.nmea, "*");

    calculateChecksum();

    strcat(espData->gpsData.nmea, "\r\n");


    if (espData->wifiCfg.state == 1)   //If ethernet running send the GPS there
    {
        int len = strlen(espData->gpsData.nmea);
        // udpMethods.udp.writeTo(nmea,len,IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        // TODO: udpMethods.udp.broadcastTo(nmea,9999);
        // espUdp->udpNtrip.broadcastTo(espData->gpsData.nmea,9999);
        espUdp->sendUDPgps(espData->gpsData.nmea);
        // Eth_udpPAOGI.beginPacket(Eth_ipDestination, portDestination);
        // Eth_udpPAOGI.write(nmea, len);
        // Eth_udpPAOGI.endPacket();
    }
}

void GPS::continuousLoop(){
    while (true){
        // myGNSS.update();
        
        while(gpsSerial->available()){
            // Serial.write(gpsSerial->read());
            parser << gpsSerial->read();
        }
        vTaskDelay(10);
    }
}

void GPS::taskHandler(void *param){
    GPS* instance = (GPS*)param;
    instance->continuousLoop();
}

void GPS::sendNTRIP(uint8_t* data, uint8_t len){
    
    gpsSerial->write(data, len);
}


