#include "GPS.h"

GPS* GPS::instance = nullptr;

GPS::GPS(ESPconfig* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial, Adafruit_MCP23X17* mcp) : parser(), rvc() , myGNSS(){
    espConfig = vars;
    
    this->gpsSerial = gpsSerial;
    this->bnoSerial = bnoSerial;
    this->mcp = mcp;
    _gpsFixIndPin = espConfig->gpioDefs.gpsFix;
    _rtkFixIndPin = espConfig->gpioDefs.rtkFix;
    instance = this;
}



void GPS::GGA_Handler() //Rec'd GGA
{
    // fix time
    parser.getArg(0, espConfig->gpsData.fixTime);

    // latitude
    parser.getArg(1, espConfig->gpsData.latitude);
    parser.getArg(2, espConfig->gpsData.latNS);

    // longitude
    parser.getArg(3, espConfig->gpsData.longitude);
    // Serial.println(longitude);
    parser.getArg(4, espConfig->gpsData.lonEW);

    // fix quality
    parser.getArg(5, espConfig->gpsData.fixQuality);

    // satellite #
    parser.getArg(6, espConfig->gpsData.numSats);

    // HDOP
    parser.getArg(7, espConfig->gpsData.HDOP);

    // altitude
    parser.getArg(8, espConfig->gpsData.altitude);

    // time of last DGPS update
    parser.getArg(12, espConfig->gpsData.ageDGPS);

    buildNmea();
}
void GPS::staticGGA_Handler(){
  if(instance){
    instance->GGA_Handler();
  }
}
void GPS::init(ESPudp* espUdp){
    this->espUdp = espUdp;
    mcp->pinMode(_gpsFixIndPin, OUTPUT);
    mcp->pinMode(_rtkFixIndPin, OUTPUT);
    mcp->digitalWrite(_gpsFixIndPin, HIGH);
    mcp->digitalWrite(_rtkFixIndPin, HIGH);
    delay(1000);
    mcp->digitalWrite(_gpsFixIndPin, LOW);
    mcp->digitalWrite(_rtkFixIndPin, LOW);
    parser.addHandler("G-GGA", staticGGA_Handler);
    // parser.setErrorHandler(errorHandler);
    // parser.addHandler("G-GGA", GPS::GGA_Handler);
    // parser.addHandler("G-VTG", VTG_Handler);
    // if (!rvc.begin(&bnoSerial)){
    //     Serial.println("BNO08x not detected");
    // }
    if (rvc.begin(bnoSerial)){
        espConfig->gpsData.imuState = 1;
    } else {
        espConfig->gpsData.imuState = 2;
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
        espConfig->gpsData.state = 2;
        Serial.println("UM980 failed to respond. Check ports and baud rates. Freezing...");
        
    } else {
        espConfig->gpsData.state = 1;
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

void GPS::calculateChecksum(void)
{
  int16_t sum = 0;
  int16_t inx = 0;
  char tmp;

  // The checksum calc starts after '$' and ends before '*'
  for (inx = 1; inx < 200; inx++)
  {
    tmp = espConfig->gpsData.nmea[inx];

    // * Indicates end of data and start of checksum
    if (tmp == '*')
    {
      break;
    }

    sum ^= tmp;    // Build checksum
  }

  byte chk = (sum >> 4);
  char hex[2] = { espConfig->gpsData.asciiHex[chk], 0 };
  strcat(espConfig->gpsData.nmea, hex);

  chk = (sum % 16);
  char hex2[2] = { espConfig->gpsData.asciiHex[chk], 0 };
  strcat(espConfig->gpsData.nmea, hex2);
}



void GPS::buildNmea()
{
    strcpy(espConfig->gpsData.nmea, "");
    strcat(espConfig->gpsData.nmea, "$PANDA,");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.fixTime);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.latitude);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.latNS);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.longitude);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.lonEW);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.fixQuality);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.numSats);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.HDOP);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.altitude);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.ageDGPS);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.speedKnots);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.imuHeading);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.imuRoll);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.imuPitch);
    strcat(espConfig->gpsData.nmea, ",");
    strcat(espConfig->gpsData.nmea, espConfig->gpsData.imuYawRate);
    strcat(espConfig->gpsData.nmea, "*");

    calculateChecksum();

    strcat(espConfig->gpsData.nmea, "\r\n");


    if (espConfig->wifiCfg.state == 1)   //If ethernet running send the GPS there
    {
        int len = strlen(espConfig->gpsData.nmea);
        // udpMethods.udp.writeTo(nmea,len,IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        // TODO: udpMethods.udp.broadcastTo(nmea,9999);
        // espUdp->udpNtrip.broadcastTo(espConfig->gpsData.nmea,9999);
        espUdp->sendUDPgps(espConfig->gpsData.nmea);
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


