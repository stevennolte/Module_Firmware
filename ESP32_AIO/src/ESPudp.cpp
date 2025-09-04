#include "ESPudp.h"
#include <Update.h>


ESPudp::ESPudp(ESPdata* vars) : udp(), udpNtrip(), udpGPS(), udpWAS(){
    espData = vars;
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

    udpJoystick.listen(8887);
    udpJoystick.onPacket([this](AsyncUDPPacket packet) {
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espData.udpTimer = millis();
        
        switch (packet.data()[3]){
          case 162:
          // if(espData->joystickData.joyStickActive){
              espData->joystickData.lastMsgRecieved = millis();
            for (uint8_t i = 0; i<8; i++){
              espData->joystickData.switchStates[i] = packet.data()[i+5];
            }
          // }
            break;
        }}
    });
    

    Serial.println("Setting up NTRIP");
    udpNtrip.listen(2233);
    udpNtrip.onPacket([this](AsyncUDPPacket packet) {
      char packetBuffer[255];
      Serial.println("Sent Ntrip");
      _gps->sendNTRIP(packet.data(), packet.length());
      // Serial2.write(packet.data(), packet.length());
       String ntripStr;
       espData->gpsData.lastNtripDataLen = packet.length();
      for (size_t i = 0; i < packet.length() && i < 64; i++) {
        char buf[4];
        sprintf(buf, "%02X ", packet.data()[i]);
        ntripStr += buf;
      }
      espData->gpsData.lastNtripData = ntripStr;
    });
    udpWAS.listen(8889);
    udpWAS.onPacket([this](AsyncUDPPacket packet){
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        switch (packet.data()[3]){
          case 180:
            //TODO: change pgn
            espData->steerData.lastWAStime = millis();
            union {
              uint32_t angle;
              uint8_t bytes[4];
            } wirelessWASunion;
            wirelessWASunion.bytes[0] = packet.data()[5];
            wirelessWASunion.bytes[1] = packet.data()[6];
            wirelessWASunion.bytes[2] = packet.data()[7];
            wirelessWASunion.bytes[3] = packet.data()[8];
            espData->steerData.byte1 = packet.data()[5];
            espData->steerData.byte2 = packet.data()[6];
            espData->steerData.byte3 = packet.data()[7];
            espData->steerData.byte4 = packet.data()[8];
            // espData->steerData.wasZeroAngle = float(espData->steerCfg.steerOffset)/float(espData->steerCfg.countsPerDeg);
            if (wirelessWASunion.angle > 2147483647){
              this->espData->steerData.absAngle  = float(wirelessWASunion.angle - 4294967295)/100.0;
            } else {
              this->espData->steerData.absAngle = float(wirelessWASunion.angle)/100.0;
            }
            this->espData->steerData.actSteerAngle = this->espData->steerData.absAngle - this->espData->steerData.wasZeroAngle;
            
            break;
        }
      }
    });
    udpGPS.listen(9999);
    Serial.println("Setting Up UDP");
    
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        // Serial.println("Received UDP");
        
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espData.udpTimer = millis();
        // Serial.println("Received AIO");  
        // Serial.println(packet.data()[3]);
        switch (packet.data()[3]){
            case 120:  //GPS reply to Hello Message, disable AIO GPS
              espData->gpsCfg.externalGPS = true;
              break;
            
            case 200:  //Hello from AgIO
              Serial.println("Hello from AgIO");
              aioReply[0] = 0x80;
              aioReply[1] = 0x81;
              aioReply[2] = espData->wifiCfg.ips[3];
              aioReply[3] = espData->wifiCfg.ips[3];
              aioReply[4] = 5;

              // // Convert actSteerAngle to bytes and store in aioReply[5] and aioReply[6]
              // union {
              //     uint16_t angle;
              //     uint8_t bytes[2];
              // } angleUnion1;
              // angleUnion1.angle = static_cast<uint16_t>(espData->steerData.actSteerAngle*100);
              aioReply[5] = static_cast<uint16_t>(espData->steerData.actSteerAngle*100) & 0xFF;
              aioReply[6] = static_cast<uint16_t>(espData->steerData.actSteerAngle*100) >> 8;
              aioReply[7] = espData->steerCfg.countsPerDeg & 0xFF;
              aioReply[8] = espData->steerCfg.countsPerDeg >> 8;
              aioReply[9] = espData->steerData.switchState;
              aioReply[10] = calcChecksum(aioReply, sizeof(aioReply));
              // Serial.println(sizeof(aioReply));
              udp.writeTo(aioReply, sizeof(aioReply), IPAddress(espData->wifiCfg.ips[0],espData->wifiCfg.ips[1], espData->wifiCfg.ips[2],255) , espData->wifiCfg.aioPort);
              // sendUDP(aioReply);
              // udp.writeTo(aioReply, sizeof(aioReply), espData->wifiCfg.moduleIP, espData->wifiCfg.aioPort);
              
              // TODO: Send back a hello packet
              break;
            case 201:
              
              this->espData->wifiCfg.ips[0] = packet.data()[7];
              this->espData->wifiCfg.ips[1] = packet.data()[8];
              this->espData->wifiCfg.ips[2] = packet.data()[9];
              espData->updateIP();
              ESP.restart();
              break;
            case 251:
        
              this->espData->steerCfg.set0 = packet.data()[5];
              this->espData->steerCfg.pulseCount = packet.data()[6];
              this->espData->steerCfg.minSpeed = packet.data()[7];
              this->espData->steerCfg.set1 = packet.data()[8];
              this->espData->steerCfg.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
              espData->updateSteer();
              break;
            case 252:
              this->espData->steerCfg.gainP = packet.data()[5];
              this->espData->steerCfg.highPWM = packet.data()[6];
              this->espData->steerCfg.lowPWM = packet.data()[7];
              this->espData->steerCfg.minPWM = packet.data()[8];
              this->espData->steerCfg.countsPerDeg = packet.data()[9];
              this->espData->steerCfg.steerOffset = packet.data()[10] << 8 | packet.data()[11];
              this->espData->steerCfg.ackermanFix = packet.data()[12];
              this->espData->steerCfg.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
             
              espData->updateSteer();
              break;
            case 254:  //GPS reply to Hello Message, disable AIO GPS
              // Serial.println("got steerdata");
              this->espData->steerData.watchdog = millis();
                union {
                  uint16_t angle;
                  uint8_t bytes[2];
                } angleUnion;
                angleUnion.bytes[0] = packet.data()[8];
                angleUnion.bytes[1] = packet.data()[9];
                if (angleUnion.angle > 32767){
                  this->espData->steerData.targetSteerAngle  = float(angleUnion.angle - 65536)/100.0;
                } else {
                  this->espData->steerData.targetSteerAngle = float(angleUnion.angle)/100.0;
                }
                this->espData->steerData.status = packet.data()[7];  
        
              break;
            case 180:  //Wireless WAS 
            //TODO: change pgn
              espData->steerData.lastWAStime = millis();
              union {
                uint32_t angle;
                uint8_t bytes[4];
              } wirelessWASunion;
              wirelessWASunion.bytes[0] = packet.data()[5];
              wirelessWASunion.bytes[1] = packet.data()[6];
              wirelessWASunion.bytes[2] = packet.data()[7];
              wirelessWASunion.bytes[3] = packet.data()[8];
              if (wirelessWASunion.angle > 32767){
                this->espData->steerData.absAngle  = float(wirelessWASunion.angle - 65536)/100.0;
              } else {
                this->espData->steerData.absAngle = float(wirelessWASunion.angle)/100.0;
              }
              this->espData->steerData.actSteerAngle = this->espData->steerData.absAngle - this->espData->steerData.wasZeroAngle;
              // this->espData->steerData.actSteerAngle = this->espData->steerData.actSteerAngle + float(espData->steerCfg.steerOffset/espData->steerCfg.countsPerDeg);
              break;
          }
        }
    });
    
}



void ESPudp::sendUDP(uint8_t* _data, size_t size) {
    Serial.println("Sent data:");
    Serial.print("\tLen: ");
    Serial.println(sizeof(_data));
    udp.writeTo(_data, sizeof(_data), IPAddress(espData->wifiCfg.ips[0],espData->wifiCfg.ips[1], espData->wifiCfg.ips[2],255) , espData->wifiCfg.aioPort);
}

void ESPudp::sendUDPgps(const char * data){
    udpGPS.broadcastTo(data,9999);
}

