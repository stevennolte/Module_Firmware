#include "ESPudp.h"
#include <Update.h>


ESPudp::ESPudp(ESPconfig* vars) : udp() {
    espConfig = vars;
}

void ESPudp::begin(){
    
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
      Serial.println("message received1");
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
          Serial.println("message received");
        //   espConfig.udpTimer = millis();
          switch (packet.data()[3]){
            case 200:  //Hello from AgIO
              // TODO: Send back a hello packet
              Serial.println("hello");
              break;
            case 201:
              
              this->espConfig->wifiCfg.ips[0] = packet.data()[7];
              this->espConfig->wifiCfg.ips[1] = packet.data()[8];
              this->espConfig->wifiCfg.ips[2] = packet.data()[9];
              espConfig->updateIP();
              ESP.restart();
              break;
            
          }
        }
    });
    
}

void ESPudp::sendSteerData(){
  uint8_t angleUDP[9];
  angleUDP[0] = 0x80;
  angleUDP[1] = 0x81;
  angleUDP[2] = 180;
  angleUDP[3] = 180;
  angleUDP[4] = 4;
  // Serial.println("41");
  // Convert sensorAngle to a 32-bit integer (scaled)
  int32_t sensorAngleInt = static_cast<int32_t>(espConfig->wasData.sensorAngle * 100);
  // Split the 32-bit integer into bytes (little-endian format)
  // Serial.println("44");
  angleUDP[5] = sensorAngleInt & 0xFF;         // LSB (byte 0)
  angleUDP[6] = (sensorAngleInt >> 8) & 0xFF;  // Byte 1
  angleUDP[7] = (sensorAngleInt >> 16) & 0xFF; // Byte 2
  angleUDP[8] = (sensorAngleInt >> 24) & 0xFF; // MSB (byte 3)
  udp.writeTo(angleUDP, 9, IPAddress(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1], espConfig->wifiCfg.ips[2], 255), 8889);
}

void ESPudp::sendUDP() {
}

