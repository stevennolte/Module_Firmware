#include "ESPudp.h"
#include <Update.h>


ESPudp::ESPudp(ESPconfig* vars) : udp(), udpNtrip(), udpGPS(){
    espConfig = vars;
}

uint8_t ESPudp::calcChecksum(uint8_t* data, size_t size){
  // Serial.print("Calculating Checksum: ");
  // Serial.println(size);  
  uint8_t checksum = 0;
  // for (int i = 0; i < size; i++){
  //   Serial.print(data[i]);
  //   Serial.print(" ");
  // }
  // Serial.println();
    for (int i = 2; i < data[4] + 5; i++){
        checksum += data[i];
    }
    return checksum;
}

void ESPudp::begin(GPS* gps){
    _gps = gps;
    Serial.println("Setting up NTRIP");
    udpNtrip.listen(2233);
    udpNtrip.onPacket([this](AsyncUDPPacket packet) {
      char packetBuffer[255];
      Serial.println("Sent Ntrip");
      _gps->sendNTRIP(packet.data(), packet.length());
      // Serial2.write(packet.data(), packet.length());
      
    });
    
    udpGPS.listen(9999);
    Serial.println("Setting Up UDP");
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        // Serial.println("Received UDP");
        
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espConfig.udpTimer = millis();
        // Serial.println("Received AIO");  
        // Serial.println(packet.data()[3]);
        switch (packet.data()[3]){
            case 120:  //GPS reply to Hello Message, disable AIO GPS
              espConfig->gpsCfg.externalGPS = true;
              break;
            
            case 200:  //Hello from AgIO
              Serial.println("Hello from AgIO");
              aioReply[0] = 0x80;
              aioReply[1] = 0x81;
              aioReply[2] = espConfig->wifiCfg.ips[3];
              aioReply[3] = espConfig->wifiCfg.ips[3];
              aioReply[4] = 5;

              // // Convert actSteerAngle to bytes and store in aioReply[5] and aioReply[6]
              // union {
              //     uint16_t angle;
              //     uint8_t bytes[2];
              // } angleUnion1;
              // angleUnion1.angle = static_cast<uint16_t>(espConfig->steerData.actSteerAngle*100);
              aioReply[5] = static_cast<uint16_t>(espConfig->steerData.actSteerAngle*100) & 0xFF;
              aioReply[6] = static_cast<uint16_t>(espConfig->steerData.actSteerAngle*100) >> 8;
              aioReply[7] = espConfig->steerCfg.countsPerDeg & 0xFF;
              aioReply[8] = espConfig->steerCfg.countsPerDeg >> 8;
              aioReply[9] = espConfig->steerData.switchState;
              aioReply[10] = calcChecksum(aioReply, sizeof(aioReply));
              // Serial.println(sizeof(aioReply));
              udp.writeTo(aioReply, sizeof(aioReply), IPAddress(espConfig->wifiCfg.ips[0],espConfig->wifiCfg.ips[1], espConfig->wifiCfg.ips[2],255) , espConfig->wifiCfg.aioPort);
              // sendUDP(aioReply);
              // udp.writeTo(aioReply, sizeof(aioReply), espConfig->wifiCfg.moduleIP, espConfig->wifiCfg.aioPort);
              
              // TODO: Send back a hello packet
              break;
            case 201:
              
              this->espConfig->wifiCfg.ips[0] = packet.data()[7];
              this->espConfig->wifiCfg.ips[1] = packet.data()[8];
              this->espConfig->wifiCfg.ips[2] = packet.data()[9];
              espConfig->updateIP();
              ESP.restart();
              break;
            case 251:
              this->espConfig->steerCfg.set0 = packet.data()[5];
              this->espConfig->steerCfg.pulseCount = packet.data()[6];
              this->espConfig->steerCfg.minSpeed = packet.data()[7];
              this->espConfig->steerCfg.set1 = packet.data()[8];
              this->espConfig->steerCfg.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
              espConfig->updateSteer();
              break;
            case 252:
              this->espConfig->steerCfg.gainP = packet.data()[5];
              this->espConfig->steerCfg.highPWM = packet.data()[6];
              this->espConfig->steerCfg.lowPWM = packet.data()[7];
              this->espConfig->steerCfg.minPWM = packet.data()[8];
              this->espConfig->steerCfg.countsPerDeg = packet.data()[9];
              this->espConfig->steerCfg.steerOffset = packet.data()[10] << 8 | packet.data()[11];
              this->espConfig->steerCfg.ackermanFix = packet.data()[12];
              this->espConfig->steerCfg.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
              espConfig->updateSteer();
              break;
            case 254:  //GPS reply to Hello Message, disable AIO GPS
              // Serial.println("got steerdata");
              this->espConfig->steerData.watchdog = millis();
                union {
                  uint16_t angle;
                  uint8_t bytes[2];
                } angleUnion;
                angleUnion.bytes[0] = packet.data()[8];
                angleUnion.bytes[1] = packet.data()[9];
                if (angleUnion.angle > 32767){
                  this->espConfig->steerData.targetSteerAngle  = float(angleUnion.angle - 65536)/100.0;
                } else {
                  this->espConfig->steerData.targetSteerAngle = float(angleUnion.angle)/100.0;
                }
                this->espConfig->steerData.status = packet.data()[7];  
        
              break;
            case 9999:  //Wireless WAS 
            //TODO: change pgn
              espConfig->steerData.lastWAStime = millis();
              union {
                uint16_t angle;
                uint8_t bytes[2];
              } wirelessWASunion;
              wirelessWASunion.bytes[0] = packet.data()[5];
              wirelessWASunion.bytes[1] = packet.data()[6];
              if (wirelessWASunion.angle > 32767){
                this->espConfig->steerData.actSteerAngle  = float(wirelessWASunion.angle - 65536)/100.0;
              } else {
                this->espConfig->steerData.actSteerAngle = float(wirelessWASunion.angle)/100.0;
              }
          }
        }
    });
    
}



void ESPudp::sendUDP(uint8_t* _data, size_t size) {
    Serial.println("Sent data:");
    Serial.print("\tLen: ");
    Serial.println(sizeof(_data));
    udp.writeTo(_data, sizeof(_data), IPAddress(espConfig->wifiCfg.ips[0],espConfig->wifiCfg.ips[1], espConfig->wifiCfg.ips[2],255) , espConfig->wifiCfg.aioPort);
}

void ESPudp::sendUDPgps(const char * data){
    udpGPS.broadcastTo(data,9999);
}

