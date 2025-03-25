#include "ESPudp.h"
#include <Update.h>


ESPudp::ESPudp(ESPconfig* vars) : udp() {
    espConfig = vars;
}

void ESPudp::begin(){
    
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espConfig.udpTimer = millis();
        
        switch (packet.data()[3]){
            // case 162: //Joystick
            //   espConfig->joystickData.lastMsgRecieved = millis();
            //   for (uint8_t i = 0; i<8; i++){
            //     espConfig->joystickData.switchStates[i] = packet.data()[i+5];
            //   }
            //   break;
            case 200:  //Hello from AgIO
              // TODO: Send back a hello packet
              break;
            case 201:
              
              this->espConfig->wifiCfg.ips[0] = packet.data()[7];
              this->espConfig->wifiCfg.ips[1] = packet.data()[8];
              this->espConfig->wifiCfg.ips[2] = packet.data()[9];
              espConfig->updateIP();
              ESP.restart();
              break;
            case 254:
              //TODO: Set Speed
              espConfig->rateData.speed = (float((packet.data()[6] << 8) |  packet.data()[5])/10.0)*0.621371;
              break;
            case 229:
              espConfig->rateData.lastSectionMsg = millis();
              uint8_t _length = packet.data()[4];
              uint8_t bitIndex = 0;
              for (size_t i = 5; i<7; i++){
                
                uint8_t _byte = packet.data()[i];
                for (int bit = 0; bit <= 7; bit++) { // Extract bits from MSB to LSB
                  espConfig->rateData.sectionStates[bitIndex] = (_byte >> bit) & 0x01; // Shift and mask to get the bit
                  bitIndex++;
                }
              }
              break;
          }
        }
    });
    
}



void ESPudp::sendUDP() {
}

