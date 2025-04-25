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
        uint32_t timeDelta;
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
              timeDelta = millis() - espConfig->rateData.distanceTraveledPrevTime;
              // if (timeDelta > 1000){
             // Limit the time delta to 1 second
                espConfig->rateData.timeDelta = timeDelta; // Update the time delta
                espConfig->rateData.distanceTraveledPrevTime = millis(); // Update the previous time
                espConfig->rateData.distanceTraveled = (espConfig->rateData.speed * 17.6)*(float(timeDelta)/1000.0); // 17.6 is the conversion factor from mph to m/s
                espConfig->rateData.sectionWidthSum = 0.0;
                for (uint8_t i = 1; i<5; i++){
                    if (espConfig->rateData.sectionStates[i] == 1){
                        espConfig->rateData.sectionWidthSum += espConfig->rateData.sectonWidth[i];
                    }
                }
                espConfig->rateData.areaCovered += float(espConfig->rateData.distanceTraveled * espConfig->rateData.sectionWidthSum)/6272640.0; // in square inches
                espConfig->rateData.speed = (float((packet.data()[6] << 8) |  packet.data()[5])/10.0)*0.621371;
              // }
              break;
            case 229:
              uint32_t messageDelta = millis() - espConfig->rateData.lastSectionMsg;
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

void ESPudp::sendUPDdata(){
   espConfig->controllerData.controllerData_t.aogByte1 = 0x80;
   espConfig->controllerData.controllerData_t.aogByte2 = 0x81;
    espConfig->controllerData.controllerData_t.sourceAddress = espConfig->wifiCfg.ips[3];
    espConfig->controllerData.controllerData_t.PGN = espConfig->wifiCfg.ips[3];
    espConfig->controllerData.controllerData_t.length = sizeof(espConfig->controllerData.controllerData_t);
}

void ESPudp::sendUDP() {
}

