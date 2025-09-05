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

void ESPudp::begin(ESPGPS* gps){
    _gps = gps;

    udpJoystick.listen(8887);
    udpJoystick.onPacket([this](AsyncUDPPacket packet) {
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espData.udpTimer = millis();
        
        switch (packet.data()[3]){
          case 162:
          // if(espData->joystickData.joyStickActive){
              espData->joystick.lastMsgRecieved = millis();
            for (uint8_t i = 0; i<8; i++){
              espData->joystick.switchStates[i] = packet.data()[i+5];
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
       espData->gps.lastNtripDataLen = packet.length();
      for (size_t i = 0; i < packet.length() && i < 64; i++) {
        char buf[4];
        sprintf(buf, "%02X ", packet.data()[i]);
        ntripStr += buf;
      }
      espData->gps.lastNtripData = ntripStr;
    });
    udpWAS.listen(8889);
    udpWAS.onPacket([this](AsyncUDPPacket packet){
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        switch (packet.data()[3]){
          case 180:
            //TODO: change pgn
            espData->steer.lastWAStime = millis();
            union {
              uint32_t angle;
              uint8_t bytes[4];
            } wirelessWASunion;
            wirelessWASunion.bytes[0] = packet.data()[5];
            wirelessWASunion.bytes[1] = packet.data()[6];
            wirelessWASunion.bytes[2] = packet.data()[7];
            wirelessWASunion.bytes[3] = packet.data()[8];
            espData->steer.byte1 = packet.data()[5];
            espData->steer.byte2 = packet.data()[6];
            espData->steer.byte3 = packet.data()[7];
            espData->steer.byte4 = packet.data()[8];
            // espData->steerData.wasZeroAngle = float(espData->steerCfg.steerOffset)/float(espData->steerCfg.countsPerDeg);
            if (wirelessWASunion.angle > 2147483647){
              this->espData->steer.absAngle  = float(wirelessWASunion.angle - 4294967295)/100.0;
            } else {
              this->espData->steer.absAngle = float(wirelessWASunion.angle)/100.0;
            }
            this->espData->steer.actSteerAngle = this->espData->steer.absAngle - this->espData->steer.wasZeroAngle;

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
              espData->gps.externalGPS = true;
              break;
            
            case 200:  //Hello from AgIO
              Serial.println("Hello from AgIO");
              aioReply[0] = 0x80;
              aioReply[1] = 0x81;
              aioReply[2] = espData->wifi.ips[3];
              aioReply[3] = espData->wifi.ips[3];
              aioReply[4] = 5;

              // // Convert actSteerAngle to bytes and store in aioReply[5] and aioReply[6]
              // union {
              //     uint16_t angle;
              //     uint8_t bytes[2];
              // } angleUnion1;
              // angleUnion1.angle = static_cast<uint16_t>(espData->steerData.actSteerAngle*100);
              aioReply[5] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) & 0xFF;
              aioReply[6] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) >> 8;
              aioReply[7] = espData->steer.countsPerDeg & 0xFF;
              aioReply[8] = espData->steer.countsPerDeg >> 8;
              aioReply[9] = espData->steer.switchState;
              aioReply[10] = calcChecksum(aioReply, sizeof(aioReply));
              // Serial.println(sizeof(aioReply));
              udp.writeTo(aioReply, sizeof(aioReply), IPAddress(espData->wifi.ips[0],espData->wifi.ips[1], espData->wifi.ips[2],255) , espData->wifi.aioPort);
              // sendUDP(aioReply);
              // udp.writeTo(aioReply, sizeof(aioReply), espData->wifiCfg.moduleIP, espData->wifiCfg.aioPort);
              
              // TODO: Send back a hello packet
              break;
            case 201:
              
              this->espData->wifi.ips[0] = packet.data()[7];
              this->espData->wifi.ips[1] = packet.data()[8];
              this->espData->wifi.ips[2] = packet.data()[9];
              espData->updateIP();
              ESP.restart();
              break;
            case 251:

              this->espData->steer.set0 = packet.data()[5];
              this->espData->steer.pulseCount = packet.data()[6];
              this->espData->steer.minSpeed = packet.data()[7];
              this->espData->steer.set1 = packet.data()[8];
              this->espData->steer.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
              espData->updateSteer();
              break;
            case 252:
              this->espData->steer.gainP = packet.data()[5];
              this->espData->steer.highPWM = packet.data()[6];
              this->espData->steer.lowPWM = packet.data()[7];
              this->espData->steer.minPWM = packet.data()[8];
              this->espData->steer.countsPerDeg = packet.data()[9];
              this->espData->steer.steerOffset = packet.data()[10] << 8 | packet.data()[11];
              this->espData->steer.ackermanFix = packet.data()[12];
              this->espData->steer.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
             
              espData->updateSteer();
              break;
            case 254:  //GPS reply to Hello Message, disable AIO GPS
              // Serial.println("got steerdata");
              this->espData->steer.watchdog = millis();
                union {
                  uint16_t angle;
                  uint8_t bytes[2];
                } angleUnion;
                angleUnion.bytes[0] = packet.data()[8];
                angleUnion.bytes[1] = packet.data()[9];
                if (angleUnion.angle > 32767){
                  this->espData->steer.targetSteerAngle  = float(angleUnion.angle - 65536)/100.0;
                } else {
                  this->espData->steer.targetSteerAngle = float(angleUnion.angle)/100.0;
                }
                this->espData->steer.status = packet.data()[7];

              break;
            case 180:  //Wireless WAS 
            //TODO: change pgn
              espData->steer.lastWAStime = millis();
              union {
                uint32_t angle;
                uint8_t bytes[4];
              } wirelessWASunion;
              wirelessWASunion.bytes[0] = packet.data()[5];
              wirelessWASunion.bytes[1] = packet.data()[6];
              wirelessWASunion.bytes[2] = packet.data()[7];
              wirelessWASunion.bytes[3] = packet.data()[8];
              if (wirelessWASunion.angle > 32767){
                this->espData->steer.absAngle  = float(wirelessWASunion.angle - 65536)/100.0;
              } else {
                this->espData->steer.absAngle = float(wirelessWASunion.angle)/100.0;
              }
              this->espData->steer.actSteerAngle = this->espData->steer.absAngle - this->espData->steer.wasZeroAngle;
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
    udp.writeTo(_data, sizeof(_data), IPAddress(espData->wifi.ips[0],espData->wifi.ips[1], espData->wifi.ips[2],255) , espData->wifi.aioPort);
}

void ESPudp::sendUDPgps(const char * data){
    udpGPS.broadcastTo(data,9999);
}

